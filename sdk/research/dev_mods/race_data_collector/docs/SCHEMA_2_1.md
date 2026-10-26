# Schema f1c.race-data/2.1 (additive over 2.0)

Date: 2026-10-20. Producer: Race Data Collector 0.3.0 (source `src/collector.cpp`).
Status: dev build only, not installed, not promoted. Native validation pending (see LIVE_TESTS_CAREER_DATA_2026-10-20.md).

Rule: every 2.0 field keeps its name and meaning. 2.1 only adds fields and sections. The exceptions are listed under "Value changes" below, because they change the value of an existing field and so a 2.0 reader can see different data.

Evidence tags: FACT / CONFIRMED = proven in `f1c_ida_decompiled\findings.jsonl` (id in brackets) or `docs\38-confirmed-offsets.md`. DERIVED = arithmetic on native fields (flag `derived` or `*Derived` present in the record). INFERENCE = unproven meaning, listed as such. UNAVAILABLE = null plus a `coverage` / `unavailableFields` reason.

## metadata
| Field | Status | Source / reason |
|---|---|---|
| `schemaVersion` = `f1c.race-data/2.1`, `exporterVersion` = `2.1`, `producer` = `Race Data Collector 0.3.0` | changed value | version bump |
| `track.trackFolder` | new, native | parent folder of the .AIW/.GDB path |
| `session.seasonYear` | new, native file read | `.GDB` header `Game Filter = AND: YYYY` or `Game Name = F1 YYYY Season` [FACT replay-season-year-gdb-2026-10-13]; null when neither proves a year |
| `session.seasonYearSource` | new | text naming the source or the unavailable reason |
| `context.aiStrength`, `context.aiRealism` | UNAVAILABLE | no proven native offset; realism not isolated (0x44F8 = TrackAggression x realism [CONFIRMED S1-passingnorm-trackaggression-latched]) |
| `context.perCarPaceScalar` | pointer | see `driverStandings[].nativePaceScalar` |
| `capture.endReason` | new | `finished`, `in_progress` (checkpoint), `red_flag`, `quit` (collector stopped or session ended), `unknown`. A crash cannot be proven natively; an `in_progress` file with no closing export is the evidence (consumer decision) |
| `capture.issues` may contain `multiplayer_mode_remote_copies_unverified` | new | mode MP captures may contain remote CO-OP car copies; not identified natively (see decisions) |
| `coverage.unavailableFields` | new | reason per field not proven (see list below) |

## driverStandings[] (added keys)
| Key | Status | Source |
|---|---|---|
| `pitVisits` | native count | every completed pit visit; `pitStops` keeps its 2.0 meaning (serviced visits only) |
| `bestLapSectors` | DERIVED | best raw I1/I2 with best lap time (see sector rule) |
| `qualifyingSectors` | UNAVAILABLE | qualifying session (type 5) not captured |
| `rcdDriverName` | native file | first line of the matched `.rcd` (exact name match, existing rule); null when no RCD matched |
| `tyreTreadRaw[4]` | CONFIRMED [ai-tyre-tread-wheel-b0-proven-20261009]; order FL,FR,RL,RR [FACT vehicle-wheel-block-order-fl-fr-rl-rr-20261013] | vehicle +0x363C/0x3734/0x382C/0x3924; 1.0 fresh, 0.75 destroyed |
| `tyreWearRemainingPct[4]` | DERIVED | `(tread-0.75)*400`, clamped 0..100 [CONFIRMED web-telemetry-wfl-is-tyre-wear-not-grip, scale] |
| `nativePaceScalar` | FACT [0x44F4-pace-scalar] | vehicle +0x44F4 baseline pace scalar (per car) |
| `nativeTrackAggressionEffective` | CONFIRMED [S1-passingnorm-trackaggression-latched] | vehicle +0x44F8; meaning is TrackAggression times realism |
| `gdbPlannedStopCount`, `gdbPlannedStopLaps[]` | FACT [f1c-gdb-pitstopstrategies-parsed-into-vehicle-struct] | int32 at +0x4588, laps at +0x4590 stride 16 (up to 4) |

Existing 2.0 `plannedPitStops` is unchanged. The GDB list is a separate, sourced field.

## lapChart[].laps[] (added keys)
| Key | Status | Source |
|---|---|---|
| `sector1`, `sector2`, `sector3` | DERIVED (`sectorsDerived: true`). Were null in 2.0 | sector1 = I1, sector2 = I2 - I1, sector3 = lap - I2. Semantics FACT [web-telemetry-sectors-cumulative-s2-20261012] (matches F1CUI f1c_calcTimesAndGaps). Offsets come from the SDK observation header, so live cross-check is pending |
| `tireChoice`, `stint` | native index; `stint` = count of tyre regimes seen so far | existing per-driver tyre fields |
| `treadRaw[4]` | CONFIRMED (see above) | raw tread at lap observation |
| `weatherAtLap` | native sample | wetness fractions and track/air temperature at the frame observing the lap |
| `componentLifeRaw[4]` | existing broker fields (engine, brakes, gearbox, suspension) | per-lap snapshot. Wear gate 0x32C8 NOT used: it is the active flag [traffic_density finding] |
| `fuelLevelRaw`, `fuelUsedSinceLastLapRaw` | native fuel level; DERIVED delta | fuel rate 0x3990 is UNAVAILABLE (not proven) |
| `gapToLeaderS`, `gapToCarAheadS`, `gapsDerived: true` | DERIVED | lap-end cumulative crossing differences, seconds. Null when the car ahead has no record for that lap |

## pitStops[] (added keys)
`tireChoiceBefore` (index at entry), `tireChangedDuringVisit`, `pitVisitIndex`, `pitReason` (`planned_lap_match` when the stop lap equals the SDK planned stop lap, otherwise UNAVAILABLE text), `pitReasonDerived: true`, `tyreWearRemainingPctAtExit[4]`, `tyreTreadRawAtExit[4]`. Scheduled, damage and penalty reasons are not proven.
`isPenalty` and `isDriveThrough` stay null (2.0).

## incidents[] / contact_observed (added keys)
| Key | Meaning |
|---|---|
| `lap` | own car lap at the sampled hit |
| `attribution` | `nearest-car` when a partner was found, else `unknown` (2.0 value) |
| `pairedDriverId`, `pairedDriver`, `pairedDistanceM` | nearest other non-SC car by broker world X/Z within `pairingRadiusM` = 25 |
| `impactPeakAccelMs2` | UNCALIBRATED: trailing 1 s peak of own-car local acceleration magnitude [FACT ai-accel-inputs-mass-live-2026-09-30, veh +0x354C/0x3550/0x3554] |
| `lapDistanceM` | vehicle +0x4044 metres along the lap [FACT f1c-lapdist-4044-not-4048] |
| `guiltyDriver`, `victimDriver` | still `""`: no blame is inferred |

## penaltyHistory[] (added keys)
`lap` (native lap at the count change), `servedStatus` = null (UNAVAILABLE), `typeAndServedMeaning`. Penalty type is not proven per vehicle; counts remain native.

## retirementDetails[] (new section, array)
One entry per DNF/DSQ car: `lapAtRetirement`, `nativeReasonCode` (raw), `lastNativeCarConditions`, `carConditionChanges[]` (lap, rawBefore, rawAfter, time), `contactsInTwoLapsBefore[]` (incidents of that car as hitter or partner from lap-2 to lap), `accelPeakBeforeRetirementMs2` (UNCALIBRATED trailing 3 s peak at the terminal transition), `reasonMeaning`. The cause is not proven. The 0 vs 6 meaning is an open conflict [dnf-reasoncode-0-vs-6-crash-meaning-conflict-2026-10-20, OPEN].

## fastestLap.sectorBests
Was `{}` in 2.0. Now `{sector1, sector2, sector3, derived: true, meaning}`: minimum derived split over all completed laps (theoretical best per sector).

## Value changes (2.0 readers see different values)
1. `lapChart[].laps[].sector1..3`: null to number (derived).
2. `fastestLap.sectorBests`: `{}` to object with numbers.
3. `incidents[].attribution`: may now be `nearest-car` (previously always `unknown`).
4. `metadata.schemaVersion`: 2.0 to 2.1. Any reader that rejects other versions must accept 2.1 (see decisions).

## Coverage: unavailable (reasons in `coverage.unavailableFields`)
wingIntactFlags (front-vs-rear-wing finding is a pit-blocking condition, not an intact flag), compoundName (brand .tbc mapping not ported), aiDifficulty (0x7D01A0 source-only), gapMetres (intra-lap metres not kept), fuelRate (0x3990 unproven), wearGate0x32C8 (active flag), qualifyingSectors, trackPositionFraction (0x4714-1740 unproven), aiStrength, aiRealism, coopRemoteCopies.
Not used on purpose: veh+0x4574 (seize flag, DISPROVEN), veh+0x4538 maxSpeed (open conflict), telemetry `cs` key (current sector, not a collision flag).
