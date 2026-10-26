# Red Flag configuration

`redflag.ini` controls automatic activation. Put it in the same directory as
the Red Flag DLL and restart the game after changing it. The DLL reads the file
once, after the exact LightLoader host and supported game layout have passed
validation. A missing setting uses the reviewed default. Invalid text uses the
default; numeric values outside the supported range are clamped. The effective
configuration is recorded in `RedFlag.log` beside the DLL.

`AutomaticEnabled=0` disables automatic Red Flags. Shipping-profile DLLs do
not contain the NUMPAD debug procedures.

`RequireManualRedFlagAcceptance=1` changes only the final authorization step.
After all automatic gates qualify, the HUD keeps the standard three-line Red
Flag warning visible. A fresh `T` press rechecks the settled-race,
cooldown, yellow and severe-pattern gates. Valid evidence starts the normal Red
Flag sequence; stale evidence cancels the prompt. Holding `T` before the prompt
does not accept it.

## Non-configurable safety gates

The INI cannot enable Red Flag in multiplayer, outside a settled race, while
paused, during another restart sequence, or on an incompatible executable
layout. Only the exact required LightLoader build can initialize the DLL; the
game EXE is not hash-bound. The player car remains excluded from automatic incident qualification.
Automatic Red Flag always requires one of two branch-0 spatial proofs from
readable AIW geometry: a sustained severe car-footprint overlap with the active
weather racing line, or the configured local group of crash-terminal cars on
main-track road geometry. Weather, general damage/car counts, a pit-lane
obstruction, a broad danger radius, or collision material cannot replace those
proofs. Tracks without readable AIW fail closed. Automatic mode and confirmed
offline state are checked before any warning is created; every mutable gate is
revalidated at the eventual automatic stop edge as well as at manual `T`
acceptance.

## Settings

| Section / key | Default | Meaning | Accepted range |
|---|---:|---|---:|
| `Activation.AutomaticEnabled` | 1 | Fire automatically when all effective gates pass. | 0 or 1 |
| `Activation.RequireNativeYellow` | 1 | Require F1C's native yellow before an automatic stop. | 0 or 1 |
| `Activation.RequireManualRedFlagAcceptance` | 0 | Require a fresh `T` press to accept a qualified automatic Red Flag; revalidate all live incident gates at the key edge. | 0 or 1 |
| `Activation.MinimumRaceDistancePercent` | 25 | Disable automatic Red Flag below this configured race-distance percentage; 0 disables only this distance gate. | 0-100 |
| `Timing.CooldownSeconds` | 120 | Re-arm delay after an automatic or manual restart sequence completes. | 10-3600 when automatic mode is on |
| `Timing.RestartCountdownSeconds` | 15 | Length of the visible `RACE RESTART IN` sequence after the race has stopped. | 5-120 |
| `Timing.ManualActivationCountdownSeconds` | 10 | Delay before Red Flag after independent manual confirmation or T acceptance of an automatic offer. Shows `RED FLAG IN X SECONDS` / `PRESS T TO CANCEL`. | 1-120 |
| `Timing.OpeningWindowSeconds` | 30 | Duration of the opening-lap exception window. | 0-600 |
| `Timing.IncidentFreshnessSeconds` | 10 | How long a confirmed car remains part of a fresh incident. | 1-120 |
| `Timing.YellowConfirmMs` | 3000 | Normal native-yellow persistence. | 500-60000 |
| `Timing.OpeningYellowConfirmMs` | 1000 | Yellow persistence during the opening window. | 500-60000 |
| `Timing.WetYellowConfirmMs` | 2000 | Maximum yellow persistence at/above `WetAssistThreshold`. | 500-60000 |
| `Timing.TerminalConfirmMs` | 4000 | Normal crash-terminal persistence. | 500-60000 |
| `Timing.StationaryConfirmMs` | 3000 | Normal stationary-damaged persistence. | 500-60000 |
| `Timing.SlowDamageConfirmMs` | 1000 | Slow severe-damage persistence. | 250-60000 |
| `Timing.OpeningIncidentConfirmMs` | 2000 | Terminal/stationary persistence during the opening window. | 500-60000 |
| `Timing.BlockedCorridorConfirmMs` | 3000 | Continuous direct racing-line obstruction persistence. | 500-60000 |
| `Timing.FinalConfirmMs` | 1000 | Final stable severe-pattern hold. | 500-60000 |
| `Timing.WetFinalConfirmMs` | 750 | Final hold at/above the wet-assist threshold. | 500-60000 |
| `Timing.StrongWetFinalConfirmMs` | 500 | Final hold at/above the strong-wet threshold. | 500-60000 |
| `Timing.PreStopNoticeSeconds` | 10 | Visible reason time before `RACE STOPPED`; the HUD shows incident details and location, not a seconds calculation. | 1-120 |
| `Locality.ClusterRadiusMeters` | 300 | Normal multi-car cluster radius. | 5-5000 |
| `Locality.OpeningClusterRadiusMeters` | 200 | Opening-window cluster radius. | 5-5000 |
| `Locality.OnTrackPileupRadiusMeters` | 40 | Maximum separation for the automatic close terminal on-track pileup proof. | 5-100 |
| `Classification.StationarySpeedMps` | 0.5 | Below this speed a car is stationary. | 0-20 |
| `Classification.SlowDamagedSpeedMps` | 8.0 | Below this speed severe moving damage can contribute. | 0.1-100 |
| `Classification.RacingLineStationarySpeedMps` | 1.0 | Maximum speed for direct-line Red Flag proof; separate from incident scoring to tolerate destroyed-car jitter. | 0.1-1.0 |
| `Classification.TerminalRacingLineJitterSpeedMps` | 3.0 | Terminal-only speed tolerance for a physics-dead wreck overlapping the racing line; never applies to active/recoverable cars. | 1.0-5.0 |
| `Classification.RacingLineOverlapMarginMeters` | 0.5 | Extra tolerance beyond the modelled 1.0 m car half-width when testing line overlap. | 0-0.75 |
| `Weather.WetAssistThreshold` | 0.90 | Wetness that selects wet yellow/final timing. | 0-1 |
| `Weather.StrongWetThreshold` | 0.96 | Wetness that selects strong-wet timing/pattern. | 0-1 |
| `Patterns.EnableBlockedCorridor` | 1 | Permit automatic Red Flag for a sustained direct main racing-line obstruction. | 0 or 1 |
| `Patterns.EnableMultiTerminal` | 1 | Permit automatic Red Flag when terminal wrecks are close together on active main-track road geometry. | 0 or 1 |
| `Patterns.MultiTerminalCars` | 2 | On-track terminal wrecks needed for that exceptional spatial proof. | 1-live field size |
| `Patterns.EnableMixedBlockage` | 1 | Permit a passive placeholder note for a terminal plus stationary-damaged cluster. | 0 or 1 |
| `Patterns.MixedTerminalCars` | 1 | Terminal cars needed for the mixed pattern. | 1-live field size |
| `Patterns.MixedStationaryDamagedCars` | 2 | Stationary-damaged cars needed for the mixed pattern. | 1-live field size |
| `Patterns.EnableStrongWetPattern` | 1 | Permit the strong-wet reduced-car placeholder pattern. | 0 or 1 |
| `Patterns.StrongWetTerminalCars` | 1 | Terminal cars needed for the strong-wet pattern. | 1-live field size |
| `Patterns.StrongWetStationaryDamagedCars` | 1 | Stationary-damaged cars needed for it. | 1-live field size |
| `Patterns.MinimumClusterCars` | 2 | Minimum contributors retained as a cluster. | 1-live field size |
| `Patterns.MinimumClusterTerminals` | 1 | Minimum terminal contributors retained as a cluster. | 1-live field size |
| `Placeholder.LogNonRedFlagIncidents` | 1 | Write one diagnostic note for a significant incident below the Red Flag threshold; 0 stays silent. | 0 or 1 |
| `Restart.YRestartMode` | `PostRedFlag` | `PostRedFlag` preserves cumulative pre-stop retirees and survivor laps on `Y`; `FullRace` leaves `Y` vanilla so laps and retirees reset. | `PostRedFlag` or `FullRace` |
| `Restart.RestartProcedure` | `Countdown` | `Countdown` uses `RestartCountdownSeconds`; `DriveToPits` orders AI to pit and waits for the player to reach the pit box. Terminal/disqualified players fall back to `Countdown`. | `Countdown` or `DriveToPits` |
| `Replay.SplitOnRedFlagRestart` | 1 | Before each Red Flag restart, use F1C's native writer through LightLoader and preserve the closed segment as the next timestamped `.Vcr` part. Replay failure is logged without reviving or altering drivers. | 0 or 1 |

`StrongWetThreshold` is raised to `WetAssistThreshold` if configured lower.
`SlowDamagedSpeedMps` is raised to `StationarySpeedMps` if configured lower.
Minimum-cluster and enabled pattern counts are raised when needed so the
configuration cannot become internally contradictory. There is no fixed car
cap in the DLL; per-car storage is allocated for the current race field.

Two spatial proofs can authorize an automatic Red Flag: a sustained damaged or
terminal direct-line blocker, or `MultiTerminalCars` terminal wrecks within
`OnTrackPileupRadiusMeters` where every counted wreck is on active branch-0 road
geometry. Grass/gravel pileups and unrelated DNFs elsewhere remain passive.
Mixed and strong-wet patterns can publish only a placeholder to `RedFlag.log`.
No below-threshold decision performs game action or invokes a Safety Car. A
healthy recoverable active stall cannot authorize Red Flag even when it is on
the line. A damaged active car must also remain below the strict speed cap and
overlap the active racing line continuously for the confirmation interval. This
path intentionally uses any game-confirmed damage because F1C can leave a
visibly wheel-less chassis at `carConditions < 2` while it remains active.
For a terminal wreck, exact racing-line overlap is authoritative even when the
broader AIW road polygon disagrees. The terminal-only jitter speed prevents a
wheel-less chassis from resetting its confirmation clock due to physics motion.

Every effective setting is written to `RedFlag.log` after safe
defaulting/clamping. Loader identity, supported layout, multiplayer/player/pit,
settled-race, pause/restart, active-damage and missing-AIW gates remain
immutable and cannot be bypassed from the INI.

`MinimumRaceDistancePercent=25` means automatic Red Flag cannot activate in
5%, 10% or 20% races. A 25%, 50% or 100% race remains eligible, but still has
to pass every incident, yellow, timing, AIW and session gate. The runtime reads
F1C's live race-distance scale and fails closed if that value is invalid.

`YRestartMode` is read once at load like every other setting. In
`PostRedFlag`, broker ABI 13 holds the later `Y` restart until the cumulative
DNF roster has been removed and lap continuity captured. If that transaction
fails, `Y` is blocked. In `FullRace`, the broker lets vanilla restart proceed
and Red Flag forgets the previous stop's roster/lap/replay bookkeeping. Red
Flag's own restart is marked separately and is never mistaken for the player's
later `Y` command.

`RestartProcedure=DriveToPits` reuses the reviewed full Red Flag flow formerly
available through NUMPAD2: surviving AI cars receive pit-retirement commands,
the field is pace-controlled without overtaking, and the restart begins only
after the player reaches the pit box and bounded track cleanup completes.
`RestartProcedure=Countdown` retains the existing timed restart. This setting
does not change `YRestartMode`; the two options control different decisions.

## Tuning guidance

Change one group at a time and validate a full race weekend. Lower confirmation
times, car counts, or yellow requirements make automatic activation more
sensitive. Larger radii combine incidents across more track distance. For a
manual-only installation, set `AutomaticEnabled=0` and leave the remaining
reviewed defaults intact.
# v70 manual control and restart checkpoints (2026-09-07)

These definitions supersede older manual-acceptance and Y descriptions below.
Restart the game after editing the INI.

| Desired behavior | Activation settings |
|---|---|
| Automatic incident control (default) | `AutomaticEnabled=1`, `ManualEnabled=0`, `RequireManualRedFlagAcceptance=0` |
| Detector proposes; T approves | `AutomaticEnabled=1`, `RequireManualRedFlagAcceptance=1` |
| Player-only initiation | `AutomaticEnabled=0`, `ManualEnabled=1` |
| Automatic plus independent player initiation | `AutomaticEnabled=1`, `ManualEnabled=1` |
| No Red Flag initiation | `AutomaticEnabled=0`, `ManualEnabled=0` |

Independent initiation: press and release T to show `RACE WILL BE STOPPED` and
`CONTINUE?`, then press T again to begin the configurable activation countdown.
`PRESS T TO CONFIRM` appears below the initial prompt. The countdown shows
`RED FLAG IN X SECONDS` and `PRESS T TO CANCEL`; another fresh T cancels it.
This also follows T acceptance of a detector offer, whose incident gates are
rechecked at expiry; cancelling that offer starts the automatic cooldown.
All T actions require the game to be foreground and no Ctrl/Alt/Shift/Windows
modifier. Returning with T held cannot confirm or cancel: release and press again.
Esc, pausing, player exit or leaving a valid offline running race cancels either
stage. An unconfirmed prompt times out after 15 seconds. Manual initiation does not require yellow flags,
incident detection, the automatic distance threshold or automatic cooldown.
Offline, settled-race, readable-session and restart-safety gates still apply.
An existing detector approval notice keeps ownership of T until resolved.

`Restart.RestartProcedure=Countdown` uses `Timing.RestartCountdownSeconds`.
`DriveToPits` orders AI to pit and waits for the player's pit box; it is not an
all-cars-arrived barrier. A terminal/disqualified player uses Countdown.

`Restart.YRestartMode=PostRedFlag` restores the latest completed Red Flag
restart's grid, laps, fuel, visibility and retiree membership. Progress and new
retirements after that checkpoint are discarded by native restart. Repeated Y
uses the same checkpoint; another completed Red Flag replaces it. A pre-RF Y
remains vanilla. Invalid or stale checkpoint identity blocks Y safely.

`FullRace` restores initial qualifying positions and lets the game's normal
restart reset the race and retirees. This is native restart behavior, not a
save-state of weather, telemetry history or replay files.

v70 RC2 is a testing candidate; these new behaviors require in-game validation
before shipping. The existing separate F1 Live HUD adapter is not included.
