# Data coverage — schema f1c.race-data/2.0 (2.1 additive: see [SCHEMA_2_1.md](SCHEMA_2_1.md))

Schema 2.1 (producer 0.3.0, 2026-10-20) keeps every 2.0 field and adds the career fields, `retirementDetails` and `coverage.unavailableFields`. Its unavailable list and value changes are in SCHEMA_2_1.md. The sections below describe 2.0 and remain accurate for every field they name.

All 41 legacy top-level sections are present. Compatibility means section/consumed-field shape, not identical semantics. Empty unavailable sections do not prove zero events; metadata.coverage records the distinction. Native fields below are source-reviewed and deterministically tested, not gameplay-validated. Raw supplemental offsets are reviewed in the SDK observation header/review.

| Section | Native implementation / qualification |
|---|---|
| metadata | Schema/producer, target hash, export time, capture identity, loss/gap/late-join flags, coverage, track/session identity |
| driverStandings | Observed position/laps/finish, identity, best timing and raw conditions; damage severity and tyre wear unknown |
| qualifyingResults | Race-start grid and native qualifying time, not a complete qualifying-session history |
| pitStops | Observed visits, lane duration, native service timers, sampled fuel added and tyre choice; penalty type unavailable |
| sessionInfo | Native race session, distance, clock and track identity |
| weather | Sampled path wetness fractions and temperatures; rain intensity and forecast unknown |
| incidents | Observed hit increments; no participant pairing or blame |
| fastestLap | Observed fastest or final native best; lap number unknown after late join may be 0 |
| personalBestLaps | Native best time snapshot |
| dnfList | Native DNF/retirement; DSQ handled separately |
| retirementReasons | Raw native reason codes and DSQ status; no inferred cause |
| fuelData | Sampled level/deltas/efficiency; SDK derived offsets remain provisional, max capacity unavailable |
| driverRatings | Exact unambiguous current-driver RCD definitions and separately named native tuning |
| sessionTiming | Native race start; qualifying duration/history unavailable |
| deletedLaps | Unavailable: F1CUI policy/history |
| championship | Empty standings; native championship mode known, name/round unknown; Narrative owns season accumulation |
| radioMessages | Unavailable: F1CUI generated commentary |
| plannedPitStops | First native scheduled stop, not full strategy |
| initialGrid | First observed grid with capture completeness caveat |
| drsZones | Unavailable: independent feature provider needed |
| sectorBoundaries | Unavailable: verified track-sector provider needed |
| teamInfo | Current field team, car, members from VEH; team nationality not inferred from driver |
| serviceMessages | Unavailable: F1CUI messages; pit events collected independently |
| positionHistory | Observed order changes, no reconstructed missing history |
| newFastestLapAlerts | Observed completed-lap records only |
| lapPositions | Observed lap crossings; gaps never backfilled |
| lapChart | Lap times/positions/crossings; raw I1/I2 retained, derived sectors unknown |
| keyEvents | Capture, driver, lap, pit, position, hit, damage/tyre/penalty changes and finish observations |
| vscPeriods | Unavailable: independent feature provider needed |
| penaltyHistory | Counter changes, no inferred penalty type/reason |
| topSpeeds | Observed maximum at nominal 10 Hz; not continuous telemetry |
| driverOfTheDay | Unavailable: editorial/algorithmic award |
| radarHistory | Unavailable: F1CUI radar state |
| blackMagicConfig | Unavailable: F1CUI configuration |
| playerConfig | Native controller-role player identity/VEH/nationality; no spectator-slot assumption |
| telemetrySnapshot | Last native position, speed, fuel, tyre choice/conditions, component life and finish state |
| trackInfoExtra | AIW/GDB paths and whitelisted track definitions |
| sessionInfoExtra | Session generation, phase, clock and observed driver count |
| livePositionOrder | Last observed ordering |
| lastLapCrossings | Native crossing-time snapshot |
| gameResults | Observed classification; official:false, not an authoritative game results-file import |

Completed summaries are placed in game-root RaceData/*.json. RaceData/recovery holds JSONL journals and incomplete snapshots. Only metadata.capture.final=true files are accepted by the optional bridge. final describes observed terminal classification, whereas completeHistory separately describes the capture; a final race can still have missing history.
