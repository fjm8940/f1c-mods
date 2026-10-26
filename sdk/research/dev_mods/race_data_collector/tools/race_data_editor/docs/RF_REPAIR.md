# Red-flag repair: profiles, evidence tiers and the Monaco worked example

The editor's red-flag engine turns a red-flag continuation capture into one
document the hosted championship manager accepts. This note records what is
verified, what is owner-delegated, and what is unknown.

## Rule profiles

`era_2001_2004` is the **owner-delegated default** for the 2002-season
captures. It is **not claimed to be the verbatim 2002 FIA regulation text**;
the 2002 regulation text itself was not found. Other profiles stay selectable:
`aggregate`, `restart_order_plain`, `restart_new_race`.

### D1 `era_2001_2004`

With `C` = the leader's completed laps when the red flag started:

| `C` | Outcome | Classification |
|---|---|---|
| `C < 2` | resumed | First part **void**. Restart is a new race over the scheduled distance, from the **original grid**. Result = restart only. Drivers out in the void part are placed after all restart participants, by original grid, as `dnf: true`, 0 laps, keeping their first-part retirement reason. |
| `2 <= C` | resumed | Result = order at the end of the final counted segment. Laps: native cumulative value if lap numbering continued; if numbering restarted, countback laps + restart laps. |
| any | stopped (owner declares) | Result = order at the end of lap `C - k`. If `C - k < 1` the tool refuses and requires an explicit owner override. |

Multiple suspensions are applied in sequence, oldest first.

Evidence tiers used throughout: **V** verified by reading workspace data or
code; **R** reported by an external source; **O** owner-delegated default.

- R: 2001 Belgian GP was stopped on lap 5; the first start was declared null
  and void, the restart used the order after lap 4, and the result consisted
  only of the restarted race (Wikipedia).
- R: aggregate two-part results were last used in 1994 (The Race).
- R: the countback wording and the two-lap/75% thresholds are documented for
  later regulations (Wikipedia list of red-flagged Formula One races).
- V: the Monaco 0.2.5 restart ran the full 39 laps with lap numbering reset,
  consistent with the `C < 2` row.

### D2 countback

With `C` = leader's completed laps at the signal, the stopped-race
classification uses the order at the end of lap `C - k`; `k` is editable
(0-2, default 1) and recorded in `metadata.repair.parameters`.

### D3 retirement clusters

A retirement is **supported** when the same driver has a `contact_observed`,
`car_condition_changed` or `pit_state_changed` key event in the same segment
inside the 120 s window ending 0.5 s before the retirement. Same-timestamp
events do not count. Supported retirements and the player's own retirement are
kept as recorded. Unsupported retirements are listed for owner review; the
owner decides. **No automatic reversal ever happens.**

Verified for Monaco's final segment: 19 of 20 retirements have prior
supporting events plus a pit entry; the player has a prior pit state change.
Segment 0's two retirements are supported, and the seven lap-0 retirements
follow contacts at ~40-136 s and a pit entry, consistent with a genuine
first-lap pile-up (see `operations\LIVE_MASS_RETIREMENTS_2026-09-15.md`).

### D4 identity scheme

- `metadata.schemaVersion` stays `f1c.race-data/2.0`.
- Continuation fields move into `metadata.repair`;
  `metadata.capture.previousCaptureId` and `continuationReason` are cleared;
  `metadata.capture.combinedClassification = true`.
- `metadata.capture.captureId = <finalSegmentCaptureId>.rf-<8 hex of sha256(
  canonical sources + decisions)>` - deterministic, distinct from any raw
  capture. Re-repairing with different decisions yields a new captureId.
- `gameResults.official` stays `false`; the issue
  `owner_reviewed_red_flag_repair` is appended.
- The manager still rejects a second capture for an already-used round, so
  corrections after upload need an alternate universe.

Consequence: the raw captureId is never reused, and the same inputs always
produce the same repaired captureId (verified: stable across runs, changes
when a decision changes).

### D5 half points

The manager awards points by position from one per-universe table and has no
per-race factor. The editor records
`metadata.repair.points = {rule: "full" | "half" | "none", reason}`. Under
`era_2001_2004` the rule is `none` if the stopped result has `C - k < 2`,
`half` if below 75% of the scheduled distance, else `full`. The wizard warns
that the manager will award full points. Manager-side half-points support is
optional work package WP7 and needs separate owner approval.

### D6 times, placeholders and fastest lap

- Void-part placeholder rows (drivers who retired before the flag in an
  earlier segment) get `totalTime "-"`, `totalTimeRaw null`, gaps `"-"`.
  Monaco shows copied clocks (34.381 s once, 105.741 s for the two void-part
  drivers); only the void-part rows are replaced, matching the plan's section 2
  oracle table, which lists native times for the restart participants.
- `2 <= C` resumed with restarted numbering: `totalTimeRaw` becomes the final
  counted segment's elapsed clock (restart-only race time), marked derived.
- `fastestLap` keeps native `author`/`time`/`timeRaw`. If `lapNumber` is 0 and
  no `lapChart` lap matches `timeRaw` within 1 ms, it stays 0 with an info
  note. For Monaco the native fastest is Sarrazin 1:21.354 while the observed
  minimum is TesterA 82.262 s on lap 19 (unobserved native finalization), so
  the lap number stays 0.
- Grid, `qualifyingResults` and `initialGrid` are restored from the first
  counted segment.

## Monaco worked example (2026-09-25 fixture)

Input: `%USERPROFILE%\Downloads\SINGLE_Monaco, 2002_20260925_144455-9388-11_20260925_144455.json`,
6,165,687 B, SHA256
`8fe125420f245b0a22295af4e15e50bf73efe2e1873b118343e8d397da5eb31b`.
Two segments: segment 0 (leader 1 lap, Coulthard and Wilson retired) and the
restart, which ran the full 39 laps with numbering reset.

`C = 1`, so the `C < 2` row applies: the first part is void and the
classification is the restart only. Positions 1-22 keep the native restart
order; Coulthard and Wilson move to the end by original grid (Wilson grid 2,
Coulthard grid 5). All grid positions are restored from segment 0.

```
python -B race_data_editor.py --rf-repair "<monaco>.json" --mode combined ^
    --profile era_2001_2004 ^
    --decisions fixtures\decisions\monaco_combined_era_2001_2004.json ^
    --out trials\editor_edits\20260925_rf_repair_evidence_monaco
```

Result: captureId `20260925_144455-9388-11.rf-f3030943`, SHA256
`827ab21d29c178695acc5834d521bf668b22e27d032c2b95a4f3486195e349f4`.
The manager accepts 24 drivers; the Season Narrative parses 24; `domain.scored`
with a 2002 table gives Panis 10 and Sarrazin 6 and nobody else scores.
`metadata.repair.points.rule = "full"`.

The exact expected-vs-actual table and consumer output are in
[../VERIFICATION.md](../VERIFICATION.md).

## Evidence tiers in the repaired file

| Field | Tier | Source |
|---|---|---|
| positions 1-22 | observed | native final-segment rows, renumbered |
| void-part rows | owner-delegated + observed | earlier segment retirement `keyEvents` and original grid |
| grid/qualifying/initialGrid | observed | first counted segment |
| laps, times (continued numbering) | observed | native values kept |
| laps, times (restarted numbering) | derived | countback + restart laps, restart clock |
| `metadata.repair` | owner-delegated | recorded decisions |

## Unknowns and limitations

- Whether the lap-0/code-0 retirement cluster is a restart artifact or genuine
  game behaviour is a judgement; the tool presents it and the owner decides.
  There is no automatic reversal.
- Lap-number continuity is detected from `restart_baseline_observed` events
  (0.2.6+), then `lapChart` first lap, then `lastLapCrossings`. If none of
  those exist, the timeline reports `unknown` and the profile falls back to
  native values with a note.
- Predecessor search uses the captureId tokens embedded in file names and then
  confirms the candidate's metadata; a predecessor whose file name lacks its
  captureId is reported as missing.
- `era_2001_2004` is an owner-delegated approximation, not regulation text.
