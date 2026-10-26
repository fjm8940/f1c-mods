# Gearbox auto trigger — dev implementation (2026-09-27)

**Status:** development trial, ``. Implemented, built
(`-Wall` clean), injected into the live game (PID 22388), and verified through
the panel's control path. Off-game tests pass (smoke + 16/16). **No live race
has been driven with AUTO enabled yet** — that is the pending human step.

Owner decisions honored (recorded in
[GEARBOX_FAILURES_TAXONOMY_AND_PLAN_2026-09-27.md](GEARBOX_FAILURES_TAXONOMY_AND_PLAN_2026-09-27.md)
§3.7): 20% default chance, race + qualifying scope, false neutral in the
weights, **never an automatic retirement**.

## What was built

### DLL (`dll\gear_failure.c`)

- **One seeded roll per live race/qualifying session.** `auto_pump()` runs on
  the existing 20 Hz worker; session detection uses `RaceActiveFlag`
  (`0x7D0308`) plus `SessionType` (`0x7C56A4`, 5 = qualifying, 7 = race).
  Session transition (re)sets the roll; the seed comes from `auto_seed` or
  the tick count. Every roll result and firing is logged.
- **Type weights** (roll hit): lost gear 45%, stuck 30%, false neutral 15%,
  no drive 10%. Lost gear picks a random gear 1..max and skip-past 70% /
  drop-to-neutral 30%.
- **Onset window:** fault fires only from `auto_earliest_lap` (default 3).
- **Last-lap guard:** a no-drive roll inside the final `auto_guard_laps`
  (default 2) of `TotalRaceLaps` (`0x8ACF8C`, CONFIRMED) is downgraded to a
  lost gear; logged.
- **New mode `MODE_FN` (false neutral):** on the next requested gear change
  the box drops to neutral once and disengages itself — recoverable, no radio
  clip (none exists for this story; documented).
- **Auto faults arm through the exact same path as the GUI `ARM`** — the
  proven phases, HUD, clunk and radio all apply unchanged. Auto never stacks
  on an already-engaged fault, and never retires the car (`dnf_enable`
  stays off unless the owner asks).
- **ABI:** 10 fields appended after the guard fields (`auto_enable`,
  `auto_chance_pm`, `auto_earliest_lap`, `auto_guard_laps`, `auto_seed` GUI
  side; `auto_rolled`, `auto_fired`, `auto_type`, `auto_lap`,
  `auto_roll_result` status side). `abi_version` 1 → 2, `struct_size` 188.

### Panel (`tools\mech_failure_test_gui.py`)

- Gearbox **Advanced** gains an "Auto trigger" section: enable checkbox,
  chance combo (5/10/20/30/50 %), earliest lap, last-lap guard, and a status
  line (waiting / rolled MISS / rolled HIT / FIRED type at lap N).
- Mode list includes "False neutral" for manual testing.
- Contract tests updated: `GEAR_F` now 47 fields; `MODE_FN == 5` asserted.

## Verification so far

| Check | Result |
|---|---|
| `gcc -shared -O2 -Wall` | exit 0, no warnings (241,188 bytes) |
| Panel smoke + unit tests | PASS, 16/16 |
| Injection + control block | attached, `hook_ok=1`, heartbeat advancing |
| New ABI live | `abi=2`, `size=188`, defaults 200/3/2, `auto_enable=0` |
| Previous binary/DLL preserved | `workspace_organization_audit\AUTO_TRIGGER_BEFORE_2026-09-27\` |

Note: the game process had restarted (new PID 22388) between sessions, so all
three DLLs were reinjected into the new process; the SDK reader followed the
new PID automatically.

## Pending human validation

1. Enable **Auto trigger** in the panel's Gearbox Advanced section, run a race,
   and confirm: roll logged once, fault type feels right, onset not before
   lap 3, no terminal inside the last 2 laps, no auto-retirement.
2. **DONE — false neutral driver-confirmed 2026-09-27 evening** (see Live
   verification below).
3. Verify a clean-session roll MISS shows in the status line and log.
4. Keep the usual driving checks (refusals, redirects, radio timing) on the
   new binary.

Watch `dll\gear_failure_log.txt` for `AUTO:` lines; the panel status line
mirrors the same state.

## Live verification — false neutral (2026-09-27 evening)

- Environment: game PID 29588 (fresh process), gear DLL injected live,
  `abi=2`, `struct_size=188`, `hook_ok=1` — the new binary confirmed running.
- Method: FN armed through the shared control block (`mode=5`,
  `cmd_arm=1`); manual arm, no roll involved.
- Result: on the driver's next shift (1→2, ~91 s after arming) the request
  was replaced with neutral: `neutral_hits 0→1`, `engaged 1→0` (one-shot
  auto-release), `symptom=1`, `last_result=0`. Driver report verbatim:
  **"shifted from 1 to 2, went to N as you intended"**.
- Recovery: the next selection engaged normally (`cur=1` on the follow-up
  read; `hits` stayed 1, no further drops).
- Status: **DRIVER-CONFIRMED** for the one-shot drop and the recovery. Not
  separately reported: the HUD flash and the refusal clunk (the banner is
  brief for a one-shot).

## Deliberately unchanged

- No auto-retire: terminal no-drive leaves the car coasting under player
  control (owner decision).
- Ranked weights and phase tables unchanged from the live-proven values.
- No public INI yet — the dev panel is the configuration surface until the
  release-candidate packaging (ship plan P3).

## Preservation

Before-states: `workspace_organization_audit\AUTO_TRIGGER_BEFORE_2026-09-27\`
(gear source + DLL, panel, tests, README, restore via copy-over).

## Follow-up (2026-09-27, same day)

- **Post-finish guard added** to `auto_pump()`: a pending auto fault is
  dropped when the player's `FinishState` (`veh+0x4064`) is non-zero, so a
  fault can never arm on the results screen. Logged once.
- **100% test option** added to the panel's Auto chance combo — validates the
  roll→fire path deterministically instead of coin-flipping.
- Rebuilt (`gcc -Wall` clean, exit 0). The game was closed at the time, so
  the new binary goes live on the next LOAD ALL. Off-game tests still pass.
- Fixes before-state: `workspace_organization_audit\AUTO_TRIGGER_FIXES_BEFORE_2026-09-27\`.
