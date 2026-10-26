# Engine auto trigger — dev implementation (2026-09-27)

**Status:** development trial, ``. Implemented, built
(`-Wall` clean), binary string-verified, off-game tests pass (smoke + 16/16).
Not yet injected or live-validated — the game was closed when this was built;
the new binary activates on the next LOAD ALL.

## Owner decisions honored (2026-09-27)

- **Per-family percentages** (independent rolls, our system): engine 15%,
  gearbox 20%, suspension 15% per race/qualifying session.
- **Engine auto types — four core:** smoke 35 / wear-to-death 35 / instant
  blow 15 / seizure 15. Oil leak and misfire cutout stay **manual-only**.
- Suspension weights also decided for its upcoming build: FAILING 45 /
  FAILURE 35 / COLLAPSED 10; wheels rear 50 / all 30 / front 20.

## What was built

### DLL (`dll\engine_failure.c`)

- 10 ABI fields appended after the guard fields (same set as the gearbox:
  `auto_enable/chance_pm/earliest_lap/guard_laps/seed/rolled/fired/type/
  lap/roll_result`); `abi_version` 1 → 2, `struct_size` 41 int-slots.
- `auto_pump()` on the 20 Hz worker: one seeded roll per live
  race/qualifying session, session gate = `RacePhase > 4` + SessionType 5/7.
  Type weights 35/35/15/15. Arms through the existing proven levers
  (`arm_engine_stage_one/terminal/instant`, `arm_seizure`).
- Onset from lap 3 (`auto_earliest_lap`).
- **Post-finish guard:** pending fault dropped when player `FinishState`
  (`veh+0x4064`) is non-zero.
- **Last-lap guard:** instant blow / seizure inside the final 2 laps
  (`TotalRaceLaps` 0x8ACF8C) are downgraded to smoke — never instant death at
  the end of a race. Never auto-retires.
- Every roll, miss, fire and downgrade logged as `AUTO:` lines in
  `engine_failure_log.txt`; status republished to the panel.

### Panel (`tools\mech_failure_test_gui.py`)

- Engine → Advanced → "Auto trigger" section (enable, chance 5–100%,
  earliest lap, no-instant-death guard, live status line mirroring roll/fire).
- `ENG_F` map extended to 41 fields; contract test updated.

## Verification so far

| Check | Result |
|---|---|
| `gcc -shared -O2 -Wall` | exit 0, no warnings (239,150 bytes) |
| Binary contains `AUTO: roll HIT` | yes |
| Panel smoke + unit tests | PASS, 16/16 |

## Preservation note (honest)

- The pre-auto engine binary was intended to be snapshotted first, but the
  copy failed on a relative-path error and the rebuild overwrote it. Its
  recovery path: rebuild from the current source **minus the documented auto
  block** (struct fields, constants, `auto_pump`, defaults, loop call), or
  use the pre-key-removal binary preserved in
  `GUI_ONLY_CONTROL_BEFORE_2026-09-27\engine_failure.dll`. The exact delta is
  listed in "What was built" above.
- Panel/README before-states: `ENGINE_AUTO_BEFORE_2026-09-27\README.md`.

## Pending human validation

1. Next game launch: LOAD ALL 3 DLLs → Engine → Advanced → Auto trigger →
   100% → race → confirm roll→fire→stages→radio, and that instant/seizure
   downgrade inside the final 2 laps.
2. Gearbox AUTO + false neutral live validation (still outstanding from the
   gearbox record).

## Next

Suspension scheduler port (same pattern, weights already decided), then the
family percentages get their first full weekend test.
