# Red Flag changelog

RED FLAG CHANGELOG

1.1.1 HOTFIX - 2026-09-08
- Fixed T pressed in another application opening or accepting a Red Flag.
  T controls now require F1 Challenge to be the active window. Returning while
  holding T does not count as a new press. Ctrl+T remains excluded.
- Added a cancellable delay after manual confirmation and after T acceptance
  of an automatic Red Flag offer. The HUD shows RED FLAG IN X SECONDS and
  PRESS T TO CANCEL. Press and release T again to cancel before activation.
- New setting: [Timing] ManualActivationCountdownSeconds=10 (1-120 seconds).
  This is separate from the countdown that restarts an already stopped race.
- Esc, pause, player exit or an unsafe session cancels the activation timer.
  Cancellation wins even at expiry. Accepted automatic incidents are checked
  again when the timer expires; a cleared incident cannot stop the race.
- Cancelling a T-accepted automatic offer applies the automatic cooldown.
- Lightloader 1.1.0 and the included ABI 13 broker are unchanged from 1.1.0.

1.1.0 - 2026-09-07
Changes compared with the public 1.0.0 release

NEW CONTROLS AND SETTINGS
- Optional player-initiated Red Flags. With ManualEnabled=1, press plain T to
  display RACE WILL BE STOPPED / CONTINUE?, release it and press T again to
  confirm. Esc, pause, leaving an eligible race or 15 seconds cancels the prompt.
- Optional approval of automatically detected incidents through
  RequireManualRedFlagAcceptance. The supplied INI enables this: press T to
  accept an offered Red Flag while the incident still qualifies. Set it to 0
  for fully automatic stops. This is separate from independent manual initiation.
- T controls accept only a fresh, unmodified press. Ctrl+T retains the game's
  speed-up function; Alt/Shift/Windows + T are also excluded.
- MinimumRaceDistancePercent limits automatic detection by configured race
  length. The supplied value 25 excludes shorter races; 0 disables this gate.
  This is the selected race length, not the percentage of the race completed.
- RestartProcedure selects Countdown or DriveToPits. The latter orders AI to
  pit and waits for the player's pit box, not every AI car's arrival. Countdown
  remains the default; its adjustable duration already existed in 1.0.0.
- YRestartMode=PostRedFlag returns to the latest completed Red Flag restart
  checkpoint, including grid, laps, fuel and retirees. Repeated Y retains that
  checkpoint; a later completed Red Flag replaces it. Later race progress is
  discarded. Also supports checkpoints from stops with no retired cars.
- YRestartMode=FullRace restores the original qualifying grid and lets the
  game's normal restart reset laps and retirees. It is not a complete rewind
  of weather or replay files. Before a Red Flag, Y keeps its normal behavior.

DETECTION AND MESSAGES
- Player-car incidents are now evaluated by the detector instead of being
  excluded. A rules-only disqualification is not itself a crash trigger.
- Active damaged cars blocking the racing line no longer need to meet the
  separate heavy-damage threshold. Damage, low speed, continuous obstruction
  and the other incident gates still apply; a harmless stationary car does not
  qualify merely by being stopped.
- Incident freshness accounts for the warning period. The mod rechecks live
  conditions before stopping, including when the player accepts an offer, to
  reject warnings that no longer qualify.
- Fully automatic warnings now use three steady lines: warning, reason and
  sector, replacing rotating detail pages. T-acceptance mode shows its own
  confirmation prompt. Countdown messages are cleared more completely.

RESTART AND CLEANUP FIXES
- Fixed red-flag icon flickering during the stop.
- Esc, player exit or reaching results cancels a pending countdown/pit-return
  restart. Eligibility is checked again after replay finalization and before
  the game consumes the restart request.
- Retired AI cars use their assigned garage position and orientation. Removed
  the legacy hiding writes that could alter the track's garage-height data.
- Improved fuel, driver visibility and individual lap restoration after the
  game initializes the restarted race. Saved values stay aligned with surviving
  cars when another car retires during the stopping sequence.
- Added a bounded, single-pass cleanup of loose debris/parts. Cleanup runs
  separately from replay and race-control restoration so slow cleanup cannot
  invalidate the restart's restoration work.
- Added roster-identity and retired/disqualified-player checks around restart
  construction and restoration. Unsafe or changed state causes refusal rather
  than guessed restoration. This does not guarantee a restart after player exit.
- Restoration timeouts no longer interrupt a callback that already owns the
  restore operation; failed post-construction restoration prevents further
  Red Flag activity until the session is exited.

COMPATIBILITY AND PACKAGING
- Added an entrant-count data bridge for an adapted F1 Live HUD. The separate
  Java adapter is NOT included; this is not automatic full HUD integration.
- Lightloader 1.1.0 is unchanged and still required. The bundled game-update
  broker is updated to ABI 13; replace that file when upgrading.
- Updated installation instructions, documented new INI settings and included
  this changelog.

ALREADY PRESENT IN 1.0.0
Automatic severe-incident detection and its enable/disable setting, configurable
countdown duration, lap-progress preservation, retired-driver results, native
cleanup, replay splitting, and offline-race-only operation were existing features.
They are not new 1.1.0 additions. Manual-only operation is new because independent
manual initiation is new.

1.0.0 - 2026-08-25
- Initial public release.
