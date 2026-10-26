# Suspension auto trigger — dev implementation (2026-09-28)

**Status:** development trial, ``. Implemented, built
(`gcc -Wall -Wextra -Werror` clean), binary string-verified, off-game tests
pass (smoke + 16/16), offline ABI round-trip verified. **Not yet injected or
live-validated** — the game was closed when this was built (the handoff's live
PID 29588 had exited; the SDK reader is healthy but reports "Game not found").
The new binary activates on the next LOAD ALL after the game starts.

## Owner decisions honored

- Per-family chances (2026-09-27): suspension **15%** per race/qualifying
  session, independent roll (engine 15 / gearbox 20 already built).
- Suspension type weights (recorded 2026-09-27, clarified 2026-09-28):
  **FAILING 45 / FAILURE 35 / COLLAPSED 10**, and by explicit owner answer the
  remaining **10% is a no-fault session** (the roll hits but no event fires).
- Wheel split: **rear 50 / all 30 / front 20**.
- Never an automatic retirement; terminal collapse downgraded inside the last
  guard laps (default 2); one fault per family per session; race + qualifying
  only; no instant arm after the player has finished.

## What was built

### DLL (`mods/sdk_sandbox/suspension_failure/suspension_failure.c`)

- **ABI 1 -> 2**, `struct_size` 236. Ten int fields appended **after `msg`**:
  `auto_enable, auto_chance_pm, auto_earliest_lap, auto_guard_laps, auto_seed`
  (GUI side) and `auto_rolled, auto_fired, auto_type, auto_lap,
  auto_roll_result` (DLL side). All pre-existing offsets, including `msg`,
  are unchanged; the panel map skips the 64 `msg` bytes (indices jump 33 -> 49).
  A `_Static_assert(sizeof(SusCtl) == 236)` in the source locks the ABI.
- `auto_pump()` on the existing 20 Hz worker: one seeded roll per live
  race/qualifying session (`session_live()` + `SESSION_TYPE` 5/7), seed from
  `auto_seed` or the tick count. Type split 45/35/10 + 10% no-fault; wheel
  split rear 50 / all 30 / front 20 (LCG draws). Defaults `auto_chance_pm=150`,
  `auto_earliest_lap=3`, `auto_guard_laps=2`.
- **Onset window** from lap 3 (player lap at `0x5EC6D8+0x403C`, same base as
  gear/engine); **post-finish guard** drops a pending fault once `FinishState`
  (`0x4064`) is non-zero; **last-lap guard** downgrades COLLAPSED to FAILING
  inside the final 2 laps of `TotalRaceLaps` (`0x8ACF8C`, CONFIRMED).
- Auto fires **only through the proven `arm_stage()` path** (same phases,
  radio eid 75, log lines as the GUI), never stacks on an already-engaged
  fault, and sets the chosen wheel mask through the same `g_wheels` variable
  (published to the panel).
- Every roll, miss, hit, no-fault, downgrade and fire is logged as `AUTO:`
  lines in `build/suspension_failure_log.txt`; status republished to the panel.

### Panel (`tools/mech_failure_test_gui.py`)

- `SUS_F` map extended to 44 fields (auto fields at byte 196 = index 49).
- Suspension -> Advanced -> "Auto trigger" section: enable checkbox, chance
  combo (5-100%, default 15), earliest lap, no-collapse guard, live status
  line mirroring waiting / rolled MISS / rolled HIT / FIRED type at lap N.
- `AUTO_TYPES_S` map added; stale-DLL mismatch is visible because the panel
  and DLL are injected together (dev-only tool).

### Tests (`tools/test_mech_failure_test_gui.py`)

- Suspension contract updated: 44 fields, `auto_enable` == 49,
  `auto_roll_result` == 58, mapped size 44*4+64. All 16 contract tests pass.

## Verification so far

| Check | Result |
|---|---|
| `python tools/f1c_mod.py build mods/sdk_sandbox/suspension_failure` | exit 0, `-Wall -Wextra -Werror` clean (build.bat flags) |
| `_Static_assert(sizeof(SusCtl) == 236)` | compiles — ABI layout pinned |
| Binary contains `AUTO:` / `no suspension event` / ABI-2 log line | yes |
| Panel smoke test | PASS |
| Panel contract tests | 16/16 PASS |
| Offline ABI round-trip (temp ctypes harness, named event + section) | PASS — 236-byte struct, panel offsets byte-exact, get/set/msg round-trip |

## Pending human validation

1. Next game launch: LOAD ALL 3 DLLs -> Suspension -> Advanced -> Auto trigger
   -> set 100% -> run a race -> confirm roll -> fire -> stages -> radio, and
   that COLLAPSED downgrades inside the final 2 laps.
2. Confirm auto wheels show up in the panel/live behaviour (rear default).
3. Confirm a clean-session MISS and a 10%-no-fault HIT both read sensibly in
   the status line and log.
4. The ordinary manual suspension checks (stages, wheels, repair, auto-reset)
   on the new ABI-2 binary.

## Deliberately unchanged

- Manual arm/repair/clear path, stage tables (2x0.30 / 3x0.60 / 30x1.00),
  radio eid 75 behaviour, auto-reset on session exit / pit return.
- No auto-retire: COLLAPSED leaves the player in control (owner decision).
- No public INI yet — the dev panel is the configuration surface until the
  release-candidate packaging (ship plan P3).

## Preservation

Before-state (copy-only): `workspace_organization_audit/SUSP_AUTO_BEFORE_2026-09-28/`
with `RESTORE.md` (SHA256 per file): sandbox source + ABI-1 DLL, panel, tests,
product README, sandbox README.

## Wasted-effort check

- Repeated reading of large material: avoided — the gearbox/engine auto pumps
  and the sandbox suspension source were read once, in targeted windows.
- Unnecessarily broad builds/tests: none; one rebuild was needed only to add
  the static assert (deliberate ABI lock, not a retry).
- Failed/retried commands: one edit attempt failed on a non-unique anchor
  ("Advanced / DLL" appears in two panels); resolved by using a unique
  surrounding block. One temp harness bug (ctypes c_char array `memmove`)
  cost one extra run; the fixed harness is the recorded offline ABI check.
- Polling/waiting: none — live validation correctly deferred because the game
  was closed.

## Next

Suspension live validation; then the family percentages get their first full
weekend test (gearbox auto and engine auto live validation are still
outstanding from their own records).
