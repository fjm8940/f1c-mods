# Car-flavor weighting — failures scale with the car's own file data (2026-09-28)

**Owner request:** make the mechanical failures occur to the player based on
the vehicle-specific files (`.veh` / `.hdv` / `engine.ini`).

**Status:** development trial, ``. Implemented in all
three DLLs behind an **off-by-default** panel toggle, built (`-Wall -Wextra
-Werror` / sandbox `-Werror` clean), binary string-verified, off-game tests
pass (smoke + 16/16), offline ABI round-trip verified. **Not yet
live-validated** (game closed at build time).

> **2026-09-28 update:** the default auto source is now the
> [vanilla lifetime timer](VANILLA_TIMER_MODE_2026-09-28.md) (owner
> directive); this flavor layer applies to the fixed-chance **roll** source
> only and is unchanged otherwise.

## 1. What the car files can honestly drive

Read together with `CAR_FILE_MECHANICAL_DATA_REFERENCE_2026-09-28.md`,
`HDV_MECHANICAL_LINK_INVENTORY_2026-09-28.md`,
`LIFETIME_SEEDS_INVESTIGATION_2026-09-27.md`,
`LIFETIME_SEED_CORRELATION_RACE_2026-09-28.md`.

| Data | File | Nature | Use for failure probability |
|---|---|---|---|
| `[ENGINE] LifetimeAvg/Var` | `engine.ini` | one pair per car; the game seeds `Normal(3A,3V)` per subsystem (engine/brakes/gearbox/suspension) and `Normal(A,V)` for player engine life; race session + mode 2 divides by the race divisor | **the only per-car reliability numbers in the files** |
| `.veh` | — | links only (hdv/sounds/graphics) | nothing mechanical |
| HDV brake block | `.hdv` | `BrakeFailure`, `BrakeWearRate`, … | disc-thickness-at-failure, not probability; consumer of the wear fields unverified |
| HDV driveline | `.hdv` | shift timing, clutch torque, `ClutchWear=0.0` | behaviour/feel knobs, not reliability |
| HDV suspension | `.hdv` | springs/dampers/travel | could scale severity later (authored mapping) |
| `*.tbc` | tyre model | wear/temp per compound | separate (future) family |
| `damage.ini` | global | collision seizure/bend thresholds | collision, not lifetime |

The game itself derives **all four** subsystem counters from the engine pair —
there are no per-subsystem lifetime tokens anywhere. So the honest statement
is: *per-car reliability data = the engine.ini lifetime pair, applied by the
game to all four subsystem slots.*

## 2. Where the numbers live at runtime (proven)

`EngineWearFailureSimulation_004ee150` writes four floats per vehicle at
`veh+0x3D70/74/78/7C` = **engine/brakes/gearbox/suspension**, each
`Normal(3*LifetimeAvg, 3*LifetimeVar)`, `/ raceDivisor` in race + mode 2
(`flt_8ACF9C`); modes 0/3 leave `FLT_MAX`. The vehicle array is
`0x5EC6D8 + n*0x48C0`, n = 0..23 (`0x5EC6E4 + 0x48C0*n` observed as the
`this` pointer in the AI ticker). The player's four copies are seeded but
never tick (`WearTickGate veh+0x32C8 = 0`) — verified live 2026-09-27/28
(bit-identical 43 s and 203 s apart while AI slots drained 1.0/s).

## 3. Design — fleet-relative reliability multiplier

One rule, applied per family, at session start (race/qualifying only, same
gate as the auto trigger):

```
mine        = player's counter for THIS family (veh+0x3D70/78/7C)
field       = mean of the same counter over AI slots 1..23
              (plausible window 100..100000; at least 6 live samples)
mult        = clamp(field / mine, 0.50, 2.00)
chance_eff  = clamp(base_chance * mult, 0, 1000)   // per-mille
```

- **Low counter = fragile car = more failures** (higher chance), exactly the
  direction the game's own AI model uses.
- **Fleet-relative, not absolute:** the owner-set family percentages (engine
  15 / gearbox 20 / suspension 15) remain the field average; individual cars
  move up or down around it. The race divisor and any season's LifetimeAvg
  scale cancel out of the ratio.
- **Different draw per family**, per session: gearbox slot scales only the
  gearbox roll, etc. The game's own four-way draw is reused, not re-invented.
- **Safety rails:** counter outside the plausible window (mode 0/3 or no
  seed) → neutral x1.00, logged; fewer than 6 live field samples → neutral,
  logged; multiplier clamped to x0.50..x2.00; toggle off → x1.00 always.
- **Honesty label:** this is OUR mechanism consuming game-seeded data. The
  files carry no player-facing reliability system; the game never consumes
  the player's frozen counters. It is not a "restoration".

## 4. What was built

### DLLs (all three, ABI 2 -> 3)

| DLL | family slot | new fields | struct size |
|---|---|---|---|
| `gear_failure.dll` | `veh+0x3D78` | `flavor_enable`, `flavor_mult_pm`, `flavor_counter` | 200 (was 188) |
| `engine_failure.dll` | `veh+0x3D70` | same | 176 (was 164) |
| sandbox `suspension_failure.dll` | `veh+0x3D7C` | same | 248 (was 236) |

- `flavor_enable` (GUI) / `flavor_mult_pm` + `flavor_counter` (DLL status).
- `flavor_compute()` runs once per live race/qualifying session (at the same
  transition the auto roll uses), logs
  `FLAVOR: counter=.. field mean=.. over N cars - mult x1.23`.
- The auto roll uses `chance_eff`; MISS/HIT log lines now print the effective
  per-mille chance. Mid-session enable logs `applies next session`.
- `_Static_assert` size locks in each DLL; build is warning-clean.

### Panel (`tools/mech_failure_test_gui.py`)

- Each Auto trigger section gains "Car flavor: scale the chance by this car's
  lifetime seed (fleet-relative)" and the status line shows
  `| car flavor x1.23` when enabled.
- Field maps extended (gear 50, engine 44, suspension 47 fields).

## 5. Verification so far

| Check | Result |
|---|---|
| gear + engine: `gcc -O2 -Wall -Wextra -Werror` | exit 0, no warnings |
| suspension: `f1c_mod.py build` (sandbox `-Werror`) | exit 0 |
| `_Static_assert` struct sizes 200/176/248 | compile-time pass |
| Binaries contain `FLAVOR:` / `field mean=` | yes (all three) |
| Panel smoke + contract tests | PASS, 16/16 |
| Offline ABI 3 round-trip (ctypes harness, suspension) | PASS — 248 bytes, panel offsets byte-exact, get/set/msg OK |

## 6. Pending live validation

1. Enable Auto + Car flavor, run a race, check the log line: counter, field
   mean, N cars, multiplier — and that the reported multiplier matches the
   status line.
2. Confirm the multiplier direction on a known-fragile vs known-robust car
   (e.g. a low-LifetimeAvg car should read x>1.00); a second race with the
   other car should read the opposite.
3. Confirm MISS/HIT log lines show the effective chance and that several
   sessions give different multipliers (per-session draw).
4. Mode 0/3 PLR failure setting → expect `counter not seeded` and x1.00.

## 7. Limitations (known, accepted for v1)

- **Stale-slot pollution:** in a race with fewer than 24 cars, unused slots
  may hold plausible values from an earlier session. The plausibility window
  and the 6-sample minimum bound the effect; the clamp bounds the outcome.
  A future session-generation check would remove this.
- **Relative, not absolute:** an all-fragile field shifts the baseline; the
  multiplier reflects position within the field, not a fixed team ranking.
- **Behaviour knobs not used yet:** HDV brake/clutch/shift timing remain
  candidates for the delayed-shift investigation and a future brakes family
  (see the HDV inventory); nothing was invented for them here.
- **Qualifying drain:** the AI counters tick 1/s in any live session; the
  compute happens at the first live tick of the session, so the field mean is
  effectively undrained. Mid-session enables do not recompute (logged).

## 8. Preservation

Before-state (copy-only): `workspace_organization_audit/FLAVOR_WEIGHT_BEFORE_2026-09-28/`
— all three DLL sources + builds, panel, tests, product README, the two docs
whose status lines were updated, sandbox README; `RESTORE.md` has per-file
SHA256.

## 9. Wasted-effort check

- Repeated reading of large material: none; the four car-file docs and the two
  decompiled functions were read once, in targeted windows.
- Unnecessarily broad builds/tests: none; two one-shot rebuilds were needed
  only for the ABI static asserts (deliberate size locks).
- Failed/retried commands: one checklist line guessed the suspension default
  block's surrounding text incorrectly (edit re-run with the real lines); one
  harness run earlier in the day had a ctypes c_char array pitfall already
  fixed — not repeated.
- Polling/waiting: none; live validation correctly deferred because the game
  was closed.
