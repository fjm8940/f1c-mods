# 26 — F1CUILive Community Memory Offsets

Extracted from `F1CUILive.jar` (5.6MB Java plugin, 2,146 classes) via `javap -p -c` bytecode decompilation.
Three key classes: `f1c_absMemoryReaderEngine`, `f1c_memoryReaderEngine`, `f1c_memoryReaderAndWriterEngine`.

The plugin uses JNA (Java Native Access) to read/write F1C process memory at runtime.
All offsets are community-discovered through runtime observation — independent validation of Ghidra static analysis.

## Vehicle/Driver Struct Layout

Base pointer: `DAT_005f0dec` (0x005F0DEC)
Driver stride: **0x48C0** (18,624 bytes)
Driver[i] address = `DAT_005f0dec + i * 0x48C0`

Tire sub-struct stride: **0xF8** (248 bytes) — confirmed: FL=0x0DF0, FR=0x0EE8, RL=0x0FE0, RR=0x10D8

### CRITICAL: Address Model (2026-07-22)

The "Offset" column below is the **F1CUI subtraction constant from the 0x4714 anchor**,
NOT a direct byte offset from the vehicle struct start. F1CUI's base pointer (0x5F0DEC)
sits at **+0x4714** into each vehicle struct (real base ~0x5EC6D8).

To compute the actual vehicle struct offset:
- For fields read via `lsub`: `actual_offset = 0x4714 - F1CUI_value`
- For fields read via `ladd`: `actual_offset = 0x4714 + F1CUI_value`

Most fields below use `lsub`. See `docs/42-f1cui-vsc-system.md` for full derivation.

| F1CUI Sub | Hex | Actual Offset | Type | Java Field Name | Description |
|--------|-----|------|-----------------|-------------|
| 180 | 0x00B4 | int | numberOfLaps | Total laps in session |
| 324 | 0x0144 | int | numberOfLaps (wheel) | Number of laps / wheel active |
| 392 | 0x0188 | int | nextChosenTires | Next tire compound choice |
| 420 | 0x01A4 | float | elapsedTime | Elapsed time in session |
| 456 | 0x01C8 | int | callToBox | Pit call flag |
| 476 | 0x01DC | float | maxSpeed | Driver max speed |
| 480 | 0x01E0 | float | timeOfNextExit | Time of next pit exit |
| 516 | 0x0204 | int | driverStatus | Status (stalled/DSQ/racing) |
| 708 | 0x02C4 | float | trackPartFromPitExit | Track position from pit exit |
| 724 | 0x02D4 | float | actualSpeed | Current vehicle speed |
| 1552 | 0x0610 | int | noOfTrackLimits | Track limit violation count |
| 1580 | 0x062C | ? | continuousNonRace | Non-race continuous update field |
| 1628 | 0x065C | float | totalTimeWithoutStartDelay | Total time minus start delay |
| 1648 | 0x0670 | float | bestI2 | Best sector 2 time |
| 1652 | 0x0674 | float | bestI1 | Best sector 1 time |
| 1656 | 0x0678 | float | bestLap | Best lap time |
| 1660 | 0x067C | float | totalTimeOnFinishLine | Total time at finish line |
| 1664 | 0x0680 | int | lapOfRun | Lap of current run/stint |
| 1700 | 0x06A4 | int | noOfPenaltiesToProcess | Pending penalties count |
| 1713 | 0x06B1 | byte | DNF_DSQ_flag | DNF/DSQ status byte |
| 1716 | 0x06B4 | int | position | Current race position |
| 1720 | 0x06B8 | float | timeOfQualySession | Qualifying session time |
| 1724 | 0x06BC | int | plannedPositionAtStart | Grid position / planned start |
| 1728 | 0x06C0 | float | lastI2 | Last sector 2 time |
| 1732 | 0x06C4 | float | lastI1 | Last sector 1 time |
| 1736 | 0x06C8 | float | lastLap | Last lap time |
| 1740 | 0x06CC | float | trackPartExact | Exact track position (0.0-1.0) |
| 1752 | 0x06D8 | int | lapNumber | Current lap number |
| 2472 | 0x09A8 | int | carConditions | Car condition flags |
| 3352 | 0x0D18 | int | gearNumber | Current gear |
| 3440 | 0x0D70 | float | EngineMaxRPM | Engine max RPM |
| 3444 | 0x0D74 | float | EngineRPM | Current engine RPM |
| 3460 | 0x0D84 | float | fuelConsumptionFactor | Fuel consumption rate |
| 3472 | 0x0D90 | float | fuelLevel | Current fuel level |
| 3480 | 0x0D98 | int | tireChoice | Current tire compound selection |
| 3568 | 0x0DF0 | float | tirePuncture_FL | Front-left tire puncture |
| 3816 | 0x0EE8 | float | tirePuncture_FR | Front-right tire puncture |
| 4064 | 0x0FE0 | float | tirePuncture_RL | Rear-left tire puncture |
| 4312 | 0x10D8 | float | tirePuncture_RR | Rear-right tire puncture |
| 4496 | 0x1190 | float | BrakeFactor | Brake input (0.0-1.0) |
| 4504 | 0x1198 | float | ThrottleFactor | Throttle input (0.0-1.0) |
| 4700 | 0x125C | float | elapsedTime (alt) | Elapsed time (alternate) |
| 4724 | 0x1274 | float | pitTime[0] | Pit stop time slot 0 |
| 4752 | 0x1290 | float | nextStintFuel | Fuel for next stint |
| 4756 | 0x1294 | float | pitTime[1] | Pit stop time slot 1 |
| 4768 | 0x12A0 | float | lastPitTime | Last pit stop duration |
| 4772 | 0x12A4 | float | pitTime[2] | Pit stop time slot 2 |
| 4776 | 0x12A8 | float | pitStopsSpeed | Pit stop speed |
| 4804 | 0x12C4 | int | pittingStatus | Pitting status / call to box |
| 4812 | 0x12CC | float | theoricNumberOfLapsWithOneLitre | Fuel efficiency |
| 4844 | 0x12EC | int | unlockChoosableTires | Tire selection unlock flag |
| 5020 | 0x139C | int | nextChosenTires (rw) | Next chosen tires (read/write) |
| 5084 | 0x13DC | float | nextStintFuel (alt) | Next stint fuel (alternate) |
| 5144 | 0x1418 | int | penalties[0] | Penalty slot 0 |
| 5148 | 0x141C | int | penalties[1] | Penalty slot 1 |
| 5152 | 0x1420 | int | penalties[2] | Penalty slot 2 |
| 13408 | 0x3460 | int | numberOfHitsInRace | Collision/hit count |
| 13996 | 0x36AC | int | rpmLimiter | RPM limiter active flag |
| 14180 | 0x3764 | long | driverMemoryAddress | Driver struct pointer in memory |
| 16128 | 0x3F00 | float | coordinatesInTrack.y (live) | Y world coordinate (live) |
| 16136 | 0x3F08 | float | coordinatesInTrack.x (live) | X world coordinate (live) |
| 16328 | 0x3FC8 | float | coordinatesInTrack.y (replay) | Y world coordinate (replay) |
| 16336 | 0x3FD0 | float | coordinatesInTrack.x (replay) | X world coordinate (replay) |
| 16380 | 0x3FFC | float | directionInTrack.y | Y direction vector |
| 16388 | 0x4004 | float | directionInTrack.x | X direction vector |
| 18000 | 0x4650 | int | isHumanInControl / isInDrsZone | Human control + DRS zone |
| 18196 | 0x4714 | ? | postProcessInput | Post-process input field |

## Global Addresses

All addresses computed as `baseMemoryAddress + offset` where baseMemoryAddress = process base (0x400000).

### Already documented in docs/12

| Address | Known Name | F1CUI Method |
|---------|-----------|--------------|
| 0x005ACB38 | PitWindowScaleFactor | updateExitFromPitFrequency |
| 0x005C329C | VehicleCount | doFillDynData |
| 0x005CC9E8 | SessionInitState | doFillDynData |
| 0x005EA560 | GlobalInstance_5EA560 | doFillDynData |
| 0x005EA5D8 | FileEnumSentinel | checkIncidentsPart1 |
| 0x007C54DC | CurrentPlayerIndex | doFillDynData, postProcessInput |
| 0x007C54E0 | near PlayerVehiclePtr | postProcessInput |
| 0x007C56A4 | SessionType | doFillDynData, updateRaceSession |
| 0x007C56A8 | RacePhase | doFillDynData |
| 0x007C91DC | DoubleCarOrientationThreshold | updatePlrLiveTimeEngine |
| 0x007CE254 | DisableHUDFlag | doFillDynData, readOtherSingleData |
| 0x007CF3C8 | TrackConditionState | postProcessInput |
| 0x007CFDC4 | DispatchModeSelector | doFillDynData |
| 0x007D017C | ConfigFloatValue | updateGameLevels |
| 0x007D01A0 | AIDifficultySlider | readGameLevel, updateGameLevel |
| 0x007D0308 | RaceActiveFlag | postProcessInput |
| 0x007D0410 | RaceMode | canElaborateDynData, postProcessInput |
| 0x007D0798 | RaceSetupScaling | doFillDynData |
| 0x007D2DB0 | RaceStandingsSortBuffer | updateRaceSession |
| 0x007D2E44 | ScoringGateEnableFlag | doFillDynData |
| 0x008AB6E4 | ScoringGate | doFillDynData |
| 0x008AC438 | AIWFilePathBuffer | postProcessInput |
| 0x008ACEE0 | ForceFeedbackBase | doFillDynData |
| 0x008ACF8C | TotalRaceLaps | doFillDynData, updateMemoryValuesBeforeQualy |
| 0x008ACF94 | RaceTimeThreshold | checkAndUpdateSessionTotalTime |
| 0x008AFEF0 | TemperatureOffset | doFillDynData |
| 0x008AFEF4 | Flt_PhysicsBrakeBalance | doFillDynData |

### NEW — not yet in docs/12

| Address | F1CUI Method | Proposed Name |
|---------|-------------|---------------|
| 0x005C7240 | updateMemoryValuesBeforeQualy1/2/3 | QualySessionMemoryFlag |
| 0x005EC1A0 | checkIncidentsPart1 | IncidentCheckState |
| 0x005F0DEC | fillSingleDynData (base pointer) | DriverDataArrayBase |
| 0x007C6384 | updatePlayerStrategyFuel | PlayerStrategyFuel_0 |
| 0x007C6F44 | updatePlayerStrategyFuel | PlayerStrategyFuel_1 |
| 0x007C6F84 | updatePlayerStrategyFuel | PlayerStrategyFuel_2 |
| 0x007C6FC4 | updatePlayerStrategyFuel | PlayerStrategyFuel_3 |
| 0x007CC4A4 | manageInstantReplay | InstantReplayControl_0 |
| 0x007CC4A8 | manageInstantReplay | InstantReplayControl_1 |
| 0x007CC764 | manageDRS | DRSControl_0 |
| 0x007CC768 | manageDRS | DRSControl_1 |
| 0x007CC9A8 | readOtherSingleData | OtherSingleDataFlag |
| 0x007CE490 | readExitFromPitNextCheck | ExitFromPitNextCheck |
| 0x007CE4AC | doFillDynData | FillDynDataFlag |
| 0x007CEBDC | updateMemoryValuesBeforeQualy | QualyUpdateFlag |
| 0x007D0014 | updateGameLevels | GameLevels_0 |
| 0x008ACEF8 | doFillDynData | DynDataNearForceFeedback |
| 0x008ACF20 | doFillDynData | DynDataRaceState |

## Cross-Validation Notes

**Confirmed matches with Ghidra static analysis:**
- Tire stride 0xF8 — matches docs/04-tire-system.md
- Vehicle struct stride 0x48C0 — matches earlier F1CUI cross-validation
- Offsets 0x674 (FuelConsumption) and 0x67C (FuelEstimate) — Ghidra names match Java `bestI1`/`totalTimeOnFinishLine` semantics (these are DIFFERENT fields that happen to share the same offset in different struct contexts)
- Offsets 0x6B4 (LaunchRPMLogic_max) and 0x6BC (RevLimitLogic) in docs/13 — the Java names `position`/`plannedPositionAtStart` indicate these offsets serve dual purpose depending on game state

**Key discoveries not in Ghidra analysis:**
- `DAT_005f0dec` = driver data array base pointer (never appeared in decompiled .c files)
- 4 player strategy fuel globals (0x007C6384–0x007C6FC4)
- DRS control pair (0x007CC764/0x007CC768) — DRS is a community mod feature, not in original game
- Instant replay control pair (0x007CC4A4/0x007CC4A8)
- World coordinates at offsets 0x3F00–0x4004 (live vs replay distinction)
- `isHumanInControl` at 0x4650 — distinguishes player vs AI control
- `numberOfHitsInRace` at 0x3460 — collision counter
- `rpmLimiter` at 0x36AC — RPM limiter flag

## Method Index

Methods in the writer class (85KB) that MODIFY game memory:

| Method | What it does |
|--------|-------------|
| setFakePuncture / removeFakePuncture | Write tire puncture values at 0x0DF0/0x0EE8/0x0FE0/0x10D8 |
| setLimitToAIEngines / removeLimitToAIEngines | Cap AI engine RPM at 0x0D70 |
| setLimitToAISpeed | Cap AI speed at 0x0D84 |
| suppressUselessPits | Prevent unnecessary pit stops via 0x139C |
| updateMemoryValuesBeforeQualy1/2/3 | Pre-qualifying session setup |
| updateMemoryValuesContinuousNonRace | Continuous non-race memory updates |
| updateMemoryValuesSessionsSkip | Skip session memory updates |
| manageDRS | DRS system control |
| manageInstantReplay | Instant replay management |
| recalcPitStopsSpeed | Recalculate pit stop speed at 0x12A8 |
| updateThePuncture | Update tire puncture state |
| updatePlrLiveTimeEngine | Player live timing |
| checkAndUpdateSessionTotalTime | Session total time tracking |
| postProcessInput | Main post-processing (reads many globals) |
