# Lifetime seed correlation at race start — all 24 cars (2026-09-28)

**Purpose:** the owner asked to review ALL cars' gearbox/suspension/brake/engine
counters in a live race and correlate them with the `engine.ini` lifetime
values, and to list any other game-file values that determine them.

## Method (clean capture)

- Race session loaded, **paused on the grid pre-green** (`RacePhase 4`) — the
  wear ticker is gated on `RacePhase > 4`, so **drain = 0**: these are the
  raw seeds.
- Frida reads (game PID 29588) of all 24 vehicle slots × 4 counters
  (`veh+0x3D70/74/78/7C`), plus `flt_8ACF9C` race divisor and failure mode.
- Session facts: `SessionType 7` (race), `mode 2` (timescaled),
  `raceDivisor = 2.02857`, `playerLife = 1767.8`.
- Driver→team mapping from the actual race files (this mod's shuffled 2002
  lineup, e.g. Schumacher→McLaren in *this* season file set).

## Model

Per slot: `seed = Normal(3 × LifetimeAvg, 3 × LifetimeVar) / divisor`
(`EngineWearFailureSimulation_004ee150`), then `observed = seed − drain`.
With 4 independent slots per car, the per-car mean residual has sd `Var/2`
under a correct hypothesis.

## Verdict: the ACTIVE (Randomizer-reduced) tree values match

| Hypothesis | mean residual (σ units) | scatter | χ²/df |
|---|---|---|---|
| **ACTIVE tree** (audit values) | **+0.27** (≈ +130 s avg) | 0.93 σ | **0.9** |
| CUSTOM +25% tree | −1.05 (≈ −25 %) | 0.75 σ | 1.6 |

The active-tree hypothesis fits with residual scatter exactly as the
independent-draw model predicts; the +25% tree is systematically rejected.
(The earlier "cars seeded at session start" reconstruction was also wrong —
`RacePhase 4` means zero drain, and the free-fit drain was accordingly ~0.)

## Full table — implied LifetimeAvg (÷3 after ×divisor) vs ACTIVE table

| Slot | Driver | Team | 4-counter avg | Implied Avg | Table Avg | z |
|---|---|---|---|---|---|---|
| 1 | M. Schumacher | McLaren | 4091 | 2767 | 3128 | −0.72 |
| 2 | Barrichello | Ferrari | 6040 | 4084 | 4232 | −0.24 |
| 3 | TesterA | Williams | 5257 | 3555 | 3838 | −0.45 |
| 4 | Trulli | Renault | 6056 | 4095 | 3045 | **+2.13** |
| 5 | Burti | Ferrari | 5724 | 3870 | 4232 | −0.57 |
| 6 | Bobbi | Minardi | 5147 | 3480 | 3269 | +0.39 |
| 7 | Button | Toyota | 5465 | 3695 | 3291 | +0.75 |
| 8 | Manning | Toyota | 4788 | 3237 | 3291 | −0.09 |
| 9 | Lasee | Jordan | 6231 | 4213 | 3378 | +1.53 |
| 10 | Sato | BAR | 3682 | 2490 | 2806 | −0.69 |
| 11 | Lotterer | Arrows | 6271 | 4240 | 3400 | +1.53 |
| 12 | Coulthard | BAR | 3648 | 2467 | 2806 | −0.75 |
| 13 | Montoya | Sauber | 6337 | 4285 | 4193 | +0.15 |
| 14 | Sarrazin | Prost | 4157 | 2811 | 3041 | −0.48 |
| 15 | Wilson | Jaguar | 3208 | 2169 | 2818 | −1.41 |
| 16 | Zonta | Jordan | 4780 | 3232 | 3378 | −0.27 |
| 17 | Panis | Prost | 5411 | 3659 | 3041 | +1.26 |
| 18 | Raikkonen | Arrows | 6219 | 4205 | 3400 | +1.47 |
| 19 | Heidfeld | McLaren | 5463 | 3694 | 3128 | +1.14 |
| 20 | Badoer | Sauber | 5853 | 3958 | 4193 | −0.36 |
| 21 | Malcharek | Minardi | 5397 | 3649 | 3269 | +0.72 |
| 22 | Alonso | Renault | 4819 | 3259 | 3045 | +0.42 |
| 23 | Massa | Jaguar | 4524 | 3059 | 2818 | +0.51 |

- One `+2σ` outlier (Trulli). At 23 cars, ~1/20 draws outside 2σ is expected;
  nothing needs re-attribution from this sample alone.
- Player slot 0 (frozen counters): avg 6570 → implied 4443 — consistent with
  an upper-mid car; his team was not identified from files.
- Within-car per-slot scatter matches `3 × LifetimeVar` (e.g. McLaren slot 1:
  draws spread ≈ 3.0k against Var 1006 × 3).

## Are there other game-file values that determine these?

**No other car-file values.** `sub_4EE150` reads only:
- `LifetimeAvg` / `LifetimeVar` — the seed pair (engine.ini `[ENGINE]`);
- `dword_7CEA5C` — the `.PLR` "Mechanical failure rate" mode: gates arming
  (only modes 1–2 seed; modes 0/3 leave FLT_MAX) and mode 2 divides;
- `dword_7C56A4` (session type) — division only in a race;
- `flt_8ACF9C` — full-laps/actual-laps ratio from the race-length setting.

Drain is a fixed 1.0/game-second in code (not file-driven), active only from
`RacePhase > 4`. `LifetimeEngineRPM` / `LifetimeOilTemp` do **not** feed these
AI counters (they drive the player's own life drain), and there are no
per-subsystem tokens anywhere.

## Evidence

  (data inline, reproducible while the session is paused); Frida session
  `session_29588_1790580866`.
- `EngineWearFailureSimulation_004ee150.c` (formula, no role gate),
  `FuelConsumptionAndWear_004ed960.c` (AI tick gate `veh+0x32C8`, RacePhase
  gate), `2002_V1_AUDIT_2026-09-16.md` (team tables),
  `operations\ENGINE_LIFETIME_2002_25PCT_APPLY_2026-09-16.md` (+25% tree).
- Complements `LIFETIME_SEEDS_INVESTIGATION_2026-09-27.md` §3 (player's
  counters seeded but frozen; verified again in the race capture).
