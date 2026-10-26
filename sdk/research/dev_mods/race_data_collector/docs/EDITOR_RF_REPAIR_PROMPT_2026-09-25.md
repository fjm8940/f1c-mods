# Race Data editor + red-flag repair — implementation prompt (2026-09-25, rev 3)

Purpose: hand the prompt body below to a coding LLM working in this workspace.
It extends the rev 2 editor prompt with two owner requirements:

1. Edit any race, including malformed results, and let the owner **declare a race
   stopped** at a red flag (not resumed), e.g.
   `%USERPROFILE%\Downloads\SINGLE_Monaco, 2002_20260925_144455-9388-11_20260925_144455.json`.
2. A **built-in parser that repairs red-flag restarted races** and their broken
   results into one combined classification that the hosted championship manager
   accepts.

Owner words (2026-09-25): "allow user to edit races such as malformed race
results or if user determined that a race is to be stopped" and "there should be
a built in parser which fixes faulty red flag restarted races and the broken
results".

Supersedes as the handoff document:
[EDITOR_TOOL_PROMPT_2026-09-25.md](EDITOR_TOOL_PROMPT_2026-09-25.md) (rev 2), which
stays the base specification and is included by reference below. Evidence:
[EDITOR_TOOL_PROMPT_REVIEW_2026-09-25.md](EDITOR_TOOL_PROMPT_REVIEW_2026-09-25.md).

---

## PROMPT BODY — give everything below this line to the implementing LLM

You are implementing a local tool in `%USERPROFILE%\Desktop\F1`. Follow the
workspace `AGENTS.md` cold start (read `README.md`, `governance\README.md`,
`governance\PIPELINE.md`, `governance\PRODUCT_STATUS.json`,
`development\AGENTS.md`, `development\mods\race_data_collector\README.md`, run
`tools\workspace\check-workspace-structure.ps1` and record the baseline, which
already has 11 errors that you must not repair).

### Scope

**Part A — Editor.** Implement everything in the prompt body of
`development\mods\race_data_collector\docs\EDITOR_TOOL_PROMPT_2026-09-25.md`
(rev 2): tkinter GUI, task views, linked `gameResults.drivers` ↔ `driverStandings`
mirror, tree/raw editors, lexeme-preserving save, validation against both
consumers, save model, tests. Read it in full; it is mandatory. This prompt
overrides rev 2 only where stated.

**Part B — Red-flag repair (new).** A parser/repair module plus a GUI wizard
("Red flag…" menu) that turns a red-flag continuation capture into one repaired,
editable race document. Two outcomes:

- **B1 Race stopped** — the owner declares the race ended at a red flag. Result
  comes from the segment before the stop, at a chosen countback lap. Later
  segments are ignored for classification but kept as evidence.
- **B2 Race resumed** — combine all counted segments into one classification.

The repair produces a *proposal* the owner reviews and edits in the normal
Part A views before saving. Nothing is applied without preview. Every decision is
recorded in the file and the manifest.

### Verified facts about red-flag captures (2026-09-25, read-only)

- Continuation captures have `metadata.capture.previousCaptureId` and/or
  `continuationReason` (`red_flag_session_transition`,
  `red_flag_observed_review_required`, `unclassified_session_restart`), plus
  `redFlagObservedInSegment`, `requiresClassificationReview`, `closeReason`,
  `issues[]` such as `linked_session_segment_not_full_race_history`.
- Earlier segments are embedded as **complete export objects** (42 section keys
  each) in `raceSegments[]`, oldest first. Some captures embed none; their
  predecessor must be found by `captureId` among `RaceData\*.json` and
  `RaceData\recovery\*.incomplete.json` (read-only), else reported missing.
- The hosted manager (`development\season_narrative\domain.py`) **rejects every
  continuation capture**: "Restart/red-flag continuation needs a combined
  classification; automatic segment merging is not supported yet." 8 of 62
  `RaceData` exports are rejected for this reason today.
- Corpus of continuation captures (fixtures; do not modify):

  | File (RaceData unless noted) | Collector | Embedded segs | Leader laps per seg | Final leader laps | DNF | Dup grid |
  |---|---|---|---|---|---|---|
  | `SINGLE_Monaco, 2002_20260925_144455-9388-11_...` (**Downloads**, primary) | 0.2.5 | 1 | [1] | 39/39 | 22/24 | yes |
  | `SINGLE_Imola, 2002_20260921_083550-16536-47_...` (6.8 MB) | 0.2.5 | 20 | [1 ×20] | 1/31 | 24/24 | yes |
  | `SINGLE_Imola, 2002_20260921_133105-22988-1_...` | 0.2.5 | 0 | – | 1/31 | 24/24 | no |
  | `SINGLE_Imola, 2002_20260922_081002-7160-1_...` | 0.2.5 | 0 | – | 31/31 | 2/24 | no |
  | `SINGLE_Imola, 2002_20260922_104809-12016-1_...` | 0.2.5 | 0 | – | 31/31 | 15/24 | no |
  | `SINGLE_Imola, 2002_20260922_115557-22412-26_...` | 0.2.5 | 2 | [2, 7] | 31/31 | 9/24 | yes |
  | `SINGLE_Imola, 2002_20260922_123108-17628-23_...` | 0.2.5 | 4 | [1, 1, 2, 3] | 31/31 | 6/24 | yes |
  | `SINGLE_Imola, 2002_20260923_101728-6164-2_...` | 0.2.6 | 1 | [0] | 31/31 | 11/24 | no |
  | `SINGLE_Imola, 2002_20260923_114934-28180-2_...` | 0.2.6 | 1 | [0] | 31/31 | 1/24 | no |

- **Monaco primary fixture** (6,165,687 B, SHA256
  `8fe125420f245b0a22295af4e15e50bf73efe2e1873b118343e8d397da5eb31b`):
  - Segment 0 (`20260925_144026-9388-1`, `final: false`, ~105.7 s): Raikkonen,
    Burti and Badoer completed lap 1; Coulthard and Wilson retired (DNF). Its grid
    is the original grid (Raikkonen 1, Wilson 2, Burti 3, Badoer 4, ...), but its
    `lapChart` is empty even for drivers with `lapsCompleted: 1`.
  - Final segment (`20260925_144455-9388-11`): `lapChart` numbering **restarts at
    1** (Raikkonen lap 1 = 88.4 s, a standing start), and the restart ran the full
    39-lap distance. Only Panis and Sarrazin finished (39 laps, with native
    finalization after the player retired on lap 27). 22 of 24 are DNF, most with
    native retirement code 0. Seven drivers retired on lap 0 between 134 s and
    202 s. Coulthard and Wilson reappear with 0 laps and `totalTimeRaw`
    105.741 s. `gridPosition` 2 is duplicated (Burti, Wilson), and
    `qualifyingResults`/`initialGrid` differ from segment 0.
    `fastestLap.lapNumber` is 0. `metadata.championshipRaceNumber` is 0.
  - Player `Tester A` has no year suffix. This is normal, not a defect.
- Whether a red-flag restart **restores lap totals** depends on the Red Flag mod
  version (`development\mods\red_flag\` README, `gr44.c`
  `restore_individual_lap_continuity`). The 0.2.5 Monaco restart did **not**
  (numbering restarted at 1). The 0.2.6 collector adds
  `restart_baseline_observed` key events with each driver's pre-restart lap,
  fuel and retirement state (`docs\RF_AUDIT_2026-09-23.md`). Detect this per
  segment from evidence; never assume it.
- Mass native retirements can be genuine game behaviour (see
  `operations\LIVE_MASS_RETIREMENTS_2026-09-15.md`). Do not auto-reverse
  retirements. Present them as owner decisions.
- Precedent for owner-directed manual correction:
  `docs\IMOLA_CLASSIFICATION_CORRECTION_2026-09-20.md`.

### B0 Segment chain loader

- Input: any export. Build the ordered chain: embedded `raceSegments[]` + the
  file itself; if `previousCaptureId` points outside, search `RaceData\` and
  `recovery\` read-only; stop on cycle or missing link and report it.
- Show a **segment timeline**: for each segment, the captureId, closeReason,
  red flag observed, duration, leader laps, running/retired counts, whether lap
  numbering continued or restarted (from `lapChart` lap numbers,
  `lapsCompleted` deltas and `restart_baseline_observed`), and grid source.
- The owner can mark segments *counted* or *ignored*. Example: Imola 083550 has 20
  one-lap aborted segments, so ignore repeated false starts.
- Per driver per segment, extract: laps completed, retired state and reason,
  lap times, best lap, position at each completed lap (`lapChart`,
  `lapPositions`, `positionHistory`, `keyEvents` `position_order`/`lap_completed`),
  and the source used. When a source is empty (Monaco segment 0 `lapChart`),
  fall back and label the evidence tier.

### B1 Race stopped (owner declares no resumption)

- Owner picks the stop segment (default: the one with `redFlagObservedInSegment`
  / `closeReason == red_flag_session_transition`) and a **countback**: the
  classification is the running order at the end of lap `L_stop − k`. `k` is an
  owner parameter (0, 1 or 2; default 1). `L_stop` is the leader's lap when the
  red flag started.
- Classified order: drivers running at the countback lap by laps then position;
  drivers retired before it follow as DNF, ordered by laps completed then
  retirement time. Drivers retired after the countback lap but before the flag
  are an explicit per-driver owner choice (classified at countback, or DNF).
- Set per driver: `finishPosition`, `lapsCompleted`, `totalTime`/`totalTimeRaw`
  = elapsed at the countback lap when known, else `"-"`/`null` with a warning,
  `gapToLeader`, `dnf`/`dsq`/`dns`, `numberOfLaps` (original scheduled distance
  unchanged; the stop is recorded separately). Grid, qualifying and initialGrid
  come from the first counted segment.
- If the leader completed fewer laps than a threshold (owner parameter, default
  2), warn that under many rule sets no result would stand. Still allow the
  owner to proceed.
- Points: the manager computes points from `finishPosition` with the universe's
  table and has **no half-points support**. Record the owner's intended points
  rule in `metadata.repair` and warn that the manager will award full points.
  Do not modify the manager.

### B2 Race resumed (combine segments)

Rule profile, chosen in the wizard and recorded:

- **aggregate** — positions by total laps across counted segments (descending),
  then aggregate time (ascending); `totalTimeRaw` = sum of per-segment elapsed
  times.
- **restart order** — final counted segment order for classified runners;
  earlier segments contribute only laps and retirements.
- **restart as new race** — first part void: classification from the final
  segment only, grid from the original grid. Earlier-segment retirements are a
  per-driver owner choice (took the restart / DNS / DNF).

Do not claim any profile matches a specific year's FIA regulations unless you
cite the regulation text in `VERIFICATION.md`; the owner picks.

Repairs, each shown as a proposal row (accept/reject per item, then editable):

1. **Laps**: if a segment's numbering restarted, `lapsCompleted` = sum over
   counted segments (minus the partial lap at the stop when the profile says
   so); if it continued, take the last value. Record the choice of source.
2. **Grid/qualifying**: restore `gridPosition` (results + standings mirror),
   `qualifyingResults` and `initialGrid` from the first counted segment. Flag
   duplicate grid positions.
3. **Earlier retirements**: a driver retired in an earlier counted segment keeps
   that DNF, laps and reason. Later placeholder rows (0 laps, copied clock) must
   not override it.
4. **Retirement plausibility**: list retirements with native code 0 and
   clusters (≥ 3 within 60 s, or on lap 0 after a restart). The owner can mark
   each as genuine or as a restart artifact. For artifacts the owner then sets
   laps/status (e.g. classified as not running, laps at the last seen value).
   No automatic reversal.
5. **Times**: rebuild `totalTime`/`totalTimeRaw`/`gapToLeader`/`gapToPrev` by the
   profile. When times are not derivable, set display `"-"` and raw `null` and
   warn. The manager then drops the total time, which is correct.
6. **Fastest lap / personal bests / top speeds**: best across counted segments,
   with `lapNumber` in combined numbering; `fastestLap.lapNumber` 0 flagged.
7. **Lap history**: build combined `lapChart` with lap numbers offset by the
   preceding counted segments' laps, so the manager's `race_evidence` lap-sum
   check aligns. Keep original per-segment charts in `raceSegments`.
8. **Retirement lists**: rebuild `dnfList` and `retirementReasons` from final
   statuses; keep reason codes from the segment where the retirement happened.
9. **Mirror**: write the final classification to both `gameResults.drivers` and
   `driverStandings`.

### Output identity and manager acceptance (owner decision, default below)

The repaired document must pass `domain.normalize` (hosted manager) and the
Season Narrative bridge. Default, clearly stated in the wizard:

- Keep `metadata.schemaVersion = "f1c.race-data/2.0"`.
- Move `previousCaptureId`, `continuationReason`, the source captureIds and file
  hashes, the red-flag flags, the profile, parameters, every accepted or rejected
  proposal, and the owner's per-driver decisions into a new
  `metadata.repair = {kind: "red_flag_stopped" | "red_flag_combined", tool,
  version, repairedAt, sources[], profile, parameters, decisions[], notes}`.
  Then clear `previousCaptureId`/`continuationReason` in `metadata.capture`
  and add `metadata.capture.combinedClassification = true`.
- Set `metadata.capture.captureId` to
  `<finalSegmentCaptureId>.rf-<8 hex of sha256(canonical sources + decisions)>`
  (deterministic, distinct from any raw capture).
- Set `gameResults.final = true` and `metadata.capture.final = true` only when
  the owner confirms the classification in the review step. Keep
  `gameResults.official = false`, `completeHistory = false`, and append issue
  `owner_reviewed_red_flag_repair` to `metadata.capture.issues`.
- Keep `raceSegments` as evidence.
- Output file name = the final segment's original file name (Season Narrative
  parses it).

If the owner rejects this default (e.g. prefers a manager-side change or the
`f1c.manual-results/1.0` schema), stop and record the decision; do not
improvise another identity scheme.

Plain edits (Part A) of a non-continuation file do **not** change captureId.

### GUI additions

- "Red flag…" wizard: chain/timeline → segment selection → B1/B2 and profile →
  parameters → proposal table (per item accept/reject, source evidence shown) →
  per-driver decisions for ambiguous cases → preview of the full classification
  with manager/Narrative consumer check → apply into the editor (undoable as one
  step) → normal save flow.
- The classification grid highlights rows changed by repair and shows each
  row's evidence tier (observed / derived / owner-decided).
- The validation panel adds: continuation fields present (manager block),
  duplicate grid, lap numbering restart detected, placeholder rows, retirement
  clusters, fastest lap lap 0, missing predecessor segment.

### CLI additions

- `--rf-inspect <file>`: print chain/timeline and detected defects as JSON.
- `--rf-repair <file> --mode stopped|combined --profile ... --params <json>
  --decisions <json> --out <dir>`: headless repair using a saved decisions file,
  so tests and the owner can reproduce a repair exactly.

### Tests (in addition to rev 2)

Temp dirs only. Hash every fixture before and after, including the Downloads
Monaco file, and fail on any change.

1. `--rf-inspect` succeeds on all 9 continuation fixtures; Monaco reports: 2
   segments, restart numbering reset, duplicate grid 2, 2 earlier DNFs
   reappearing as placeholders, and a lap-0 retirement cluster.
2. Monaco B1 (stop after segment 0, k=0 and k=1) and B2 (each profile) with a
   checked-in decisions file: output passes `domain.normalize` and the
   Season Narrative bridge. The manager-visible classification matches an
   expected table written into the test by hand from the timeline, not generated
   by the code under test.
3. Imola `115557-22412-26` (segments with 2 and 7 leader laps) combined: summed
   laps, restored grid, combined lap numbering, manager lap-sum check aligned.
4. Imola `083550-16536-47`: ignoring 19 false-start segments works; deterministic
   captureId stable across runs, and it changes when a decision changes.
5. Missing-predecessor fixtures (0 embedded segments) report the gap and allow
   B1/B2 only on available segments with an explicit warning.
6. Round-trip: a repaired file reopened and saved with no edits is
   byte-identical.
7. No repair runs without explicit accept; rejecting all proposals leaves the
   document unchanged.

### Non-goals (additions)

- No changes to the collector, the Red Flag mod, the hosted manager or Season
  Narrative code. If the manager gate itself should change, document it as a
  follow-up.
- No writes to `RaceData\`, `recovery\`, `Downloads\` sources or the VPS.
- No invented telemetry. Derived values are labelled derived. Unknown stays
  unknown (`"-"`/`null`) with a warning.

### Documentation and reporting

As rev 2, plus: `docs\RF_REPAIR.md` in the tool folder explaining profiles,
evidence tiers, the identity scheme and its manager rationale, and the Monaco
worked example. Record in `VERIFICATION.md` the fixture hashes, decisions files
used, consumer-check outputs and the expected-vs-actual classification tables.
Report pending owner validation: pick the rule profile for Monaco, review the
proposed classification, upload to a **test universe** in the hosted manager,
confirm import and standings.

## END PROMPT BODY

Owner decisions still open (the implementer must surface them, not guess):
default rule profile for 2002 races; countback `k`; whether retirement code 0
clusters are artifacts; acceptance of the `metadata.repair` / derived-captureId
scheme for the hosted manager.
