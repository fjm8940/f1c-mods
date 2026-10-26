# Imola 2002 classification correction — 2026-09-20

Status: applied to one owner-supplied export copy; not imported anywhere; human validation pending.

## Instruction

Matias: "swap it so that Lasee is 5th, TesterA 6th and so forth — due to technical issues Lasee retired and TesterA got 5th, that is not what should be."

The native capture recorded Marcel Lasee as a lap-30 technical retirement classified 19th, so Tester A inherited 5th. The owner-directed classification is Lasee 5th and every following finisher one position lower. This is a manual classification correction, not new telemetry.

## Artifact

| | |
|---|---|
| File edited | `%USERPROFILE%\Downloads\SINGLE_Imola, 2002_20260920_154821-19304-1_20260920_154821.json` |
| Capture id | `20260920_154821-19304-1` (unchanged) |
| Before SHA256 | `e3307131700e60a39cf2ae65f746d227e9ad1290f8bf154b587ace42938d119b` |
| After SHA256 | `0e887a621dd5a5c5589280a34d49c2f3fb21fe5090ca94aa0451314f76b5b4f8` |
| Before-state backup | `%USERPROFILE%\Downloads\SINGLE_Imola, 2002_20260920_154821-19304-1_20260920_154821.BEFORE_CLASSIFICATION_FIX_20260920_195704.json` (byte-identical to the before hash) |

The file is a copy outside the game tree. The game RaceData folder currently holds a different, older Imola capture (`SINGLE_Imola, 2002_20260916_190304-21868-1_20260916_190304.json`); it was not touched.

## Change

Positions 5–19 only; positions 1–4 and 20–24 are unchanged.

| Driver | Was | Now |
|---|---|---|
| Marcel Lasee (2002) | 19 | 5 |
| Tester A | 5 | 6 |
| Takuma Sato (2002) | 6 | 7 |
| Andre Lotterer (2002) | 7 | 8 |
| David Coulthard (2002) | 8 | 9 |
| Kimi Raikkonen (2002) | 9 | 10 |
| Luca Badoer (2002) | 10 | 11 |
| Justin Wilson (2002) | 11 | 12 |
| Darren Manning (2002) | 12 | 13 |
| Ricardo Zonta (2002) | 13 | 14 |
| Jenson Button (2002) | 14 | 15 |
| Jarno Trulli (2002) | 15 | 16 |
| Matteo Bobbi (2002) | 16 | 17 |
| Jirko Malcharek (2002) | 17 | 18 |
| Felipe Massa (2002) | 18 | 19 |

Changed fields, in `driverStandings` and its copy `gameResults.drivers`:

- classification array order, `finishPosition`, and the derived `positionsGained`/`positionsLost` (grid to finish; e.g. Lasee grid 13 → 5 = gained 8, TesterA grid 11 → 6 = gained 5).

Added `metadata.classificationCorrection` recording date, authority, method, reason and the per-driver from→to mapping.

Deliberately unchanged (observed facts):

- `dnf: true`, `rawRetiredFlag`, `dnfList`, `retirementReasons` — Lasee did retire; he is classified under the 90%-distance rule, which Season Narrative `domain.scored()` already implements (a retirement with laps ≥ 90% of the winner's is classified).
- lap and timing history: `lapChart`, `lapPositions`, `positionHistory`, `lastLapCrossings`, `newFastestLapAlerts`.
- game-observed ordering and per-driver data: `livePositionOrder` (still shows the game's original last ordering), `telemetrySnapshot`, `topSpeeds`, `driverRatings`, `fuelData`, `incidents`, `pitStops`, `initialGrid`, `qualifyingResults`, `fastestLap`.

## Verification

- Serializer fidelity: the export was re-serialized with the native writer's float style (`%.17g`); an unmodified round-trip of the original is byte-identical, so only intended text changed.
- Parsed-equality checks: `driverStandings == gameResults.drivers` before and after; positions 1–24 unique; order matches the intended mapping; `positionsGained`/`positionsLost` recomputed and re-checked; every non-position field of every row equal; a full parsed comparison proves only `driverStandings`, `gameResults.drivers` and `metadata` differ from the backup.
- The written file re-parses cleanly; the backup is byte-identical to the before-state.

## Limitations and follow-up

- Manual correction by owner instruction. The underlying race remains a native observation; Season Narrative will still warn that the classification is collector-observed, not steward-certified.
- Not copied into the game RaceData folder and not verified against the hosted championship database. Import the corrected file only. Season Narrative rejects re-importing an already-finalized capture and expects a manual/synthetic correction path instead.
- `livePositionOrder` still shows the game's original ordering (TesterA 5th). Making the final live-order snapshot match the corrected classification is a separate edit and was not performed.
