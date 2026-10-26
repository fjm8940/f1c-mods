# Vanilla lifetime timer mode — player failure on the game's own countdown (2026-09-28)

**Owner directive:** "for now we should use the vanilla game timer values by
simply re-enabling them for player." The result: the player's own game-seeded
lifetime counters now count down at the game's own rate, and the family fault
fires when the counter expires — the same mechanism vanilla uses for AI cars.

**Status:** development trial, ``. Built in all three
DLLs (`-Wall -Wextra -Werror` clean), binary string-verified, off-game tests
pass (smoke + 16/16), ABI 4 round-trip byte-exact. **Not yet live-validated**
(game closed).

## 1. Why "simply re-enabling" is done by us, not by a game flag

- The game seeds four counters per vehicle at `veh+0x3D70/74/78/7C`
  (engine/brakes/gearbox/suspension) = `Normal(3*LifetimeAvg, 3*LifetimeVar)`
  from `engine.ini`, divided by the race divisor in race + mode 2
  (`EngineWearFailureSimulation_004ee150`).
- The ticker (`FuelConsumptionAndWear_004ed960`) is called only for vehicles
  whose role byte `veh+0x32C8` is 1 (AI). The player's byte is 0 by
  construction. **Flipping that byte is the only true "native gate", and it is
  not safe** — the same byte gates AI driving/role behaviour across the game
  (e.g. collision handling branches on it). It must never be flipped for the
  player car.
- Therefore the tick is driven by our DLL at exactly the game's AI rate:
  **1.0 per game-second, from `RacePhase > 4` (green)**, using the global
  session clock `dbl_8AFEA0` for elapsed time. Values, formula, mode gates and
  timing are the game's; the tick and the presentation are ours.

## 2. Behaviour

- One counter per family, per session: gearbox = `veh+0x3D78`, engine =
  `veh+0x3D70`, suspension = `veh+0x3D7C`. Brakes (`0x3D74`) has no family
  yet; its counter is ignored. **Owner directive 2026-09-28: a BRAKES family
  stays on the books as pending future work** (the strongest candidate is the
  HDV per-wheel brake wear/failure block — see
  `HDV_MECHANICAL_LINK_INVENTORY_2026-09-28.md` §1 — and the unused
  `veh+0x3D74` timer would then feed it).
- The counter is written down each tick (plain float store — the field is
  inert for the player; writing it is proven safe), so the value is visible
  and the log/status show a live countdown.
- On expiry the family fault arms through the same proven machinery as the
  roll (`auto_fire` → `arm_stage` / engine levers / gearbox arm path) with the
  family's existing type weights:
  - engine 35/35/15/15 (smoke / wear / instant / seizure), instant/seizure
    downgraded to smoke inside the final 2 laps;
  - gearbox 45/30/15/10 (lost / stuck / false neutral / no drive), no-drive
    downgraded to lost inside the final 2 laps;
  - suspension 45/35/20 on timer expiry (the roll's 10% no-fault slice is
    dropped because expiry itself is the failure), COLLAPSED downgraded to
    FAILING inside the final 2 laps.
- One fault per family per session; never auto-retire; post-finish guard;
  if a family fault is already engaged the fire waits for it to clear.
- **Not seeded** (PLR failure mode 0/3, counter `FLT_MAX`) → idle, logged,
  panel shows `vanilla timer not seeded`.
- Short races: the game divides the counters by the race-length divisor, so a
  short race gets a small counter and can fail early — exactly like vanilla
  AI. A 100% race usually sees nothing expire; that is authentic.

## 3. What was built

| DLL | timer fields (appended) | ABI | struct size |
|---|---|---|---|
| `gear_failure.dll` | `timer_enable`, `timer_remaining` | 3 -> 4 | 208 (was 200) |
| `engine_failure.dll` | same | 3 -> 4 | 184 (was 176) |
| sandbox `suspension_failure.dll` | same | 3 -> 4 | 256 (was 248) |

- New defaults in each DLL: `timer_enable = 1` (vanilla timer is the default
  auto source now), `timer_remaining = -1`.
- The fixed-chance roll and the car-flavor layer remain selectable
  (`timer_enable = 0` restores them unchanged).
- `_Static_assert` size locks; warning-clean builds; panel maps extended
  (gear 52, engine 46, suspension 49 fields).

## 4. Panel

- Each Auto trigger section gains "Use the vanilla lifetime timer (car's own
  game values; uncheck for the fixed-chance roll)", default checked.
- Status line shows the live countdown: `auto: vanilla timer 4231s to
  failure` / `expired - fault pending` / `FIRED <type> at lap N (vanilla
  timer)` / `not seeded (PLR mode 0/3?)`.

## 5. Verification so far

| Check | Result |
|---|---|
| gear + engine: `gcc -O2 -Wall -Wextra -Werror` | exit 0, no warnings |
| suspension: `f1c_mod.py build` (sandbox `-Werror`) | exit 0 |
| `_Static_assert` sizes 208/184/256 | compile-time pass |
| Binaries contain `TIMER:` / `counter expired` | yes (all three) |
| Panel smoke + contract tests | PASS, 16/16 |
| ABI 4 offline round-trip (ctypes harness, suspension) | PASS — 256 bytes, offsets byte-exact, get/set/msg OK |

## 6. Pending live validation

1. Race with Auto on (timer default): panel should count down from the seeded
   value; confirm 1 s per real second while running and that pausing does not
   obviously change the rate (session-clock driven).
2. Short race (small divisor): confirm earlier expiry, matching vanilla AI
   timing.
3. On expiry: confirm FIRED type, stages/radio, and the last-lap downgrade.
4. PLR failure mode 0/3: confirm `not seeded` + idle, no faults.
5. Cross-check one session's seeded counter against the car's `engine.ini`
   `LifetimeAvg/Var` (the correlation method in
   `LIFETIME_SEED_CORRELATION_RACE_2026-09-28.md`).

## 7. Relationship to the earlier layers

- **This is now the default auto source** (owner directive, "for now"), which
  supersedes the fixed-chance roll as the primary mechanism. The roll and the
  car-flavor weighting remain in the binary, selectable and unchanged; the
  flavor layer applies only to the roll source.
- Note vs the 2026-09-27 owner record ("probability is ours, never a
  countdown"): this is a deliberate, explicitly requested exception for now,
  not a silent reversal. The presentation, staging and retirement rules stay
  ours; only the failure *timing* source changes.

## 8. Preservation

Before-state (copy-only): `workspace_organization_audit/TIMER_MODE_BEFORE_2026-09-28/`
— all three DLL sources + builds, panel, tests, product README, both design
docs, sandbox README; `RESTORE.md` has per-file SHA256.

## 9. Wasted-effort check

- Repeated reading of large material: none — the two lifetime docs and the
  seeder/ticker functions were re-read once, only the parts needed to confirm
  counter addresses, type (float) and the gate.
- Unnecessarily broad builds/tests: none; one rebuild per DLL per change
  round, all one-shot.
- Failed/retried commands: none material; the earlier edit-anchor guess in
  the prior task was avoided here by reading the exact block first.
- Polling/waiting: none; live validation correctly deferred (game closed).
