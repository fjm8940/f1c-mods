# PACE_MULTIPLIER — D4: 0x453C dynamics and the corner-speed constants (BATCH B item 2)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Script: `phase3/analysis/d4_pace.py`
`watch_race4fullc` + `watch_race4fulld`, wet Imola `watch_20261002T194606Z`.

## 1. Static model (re-read from `map_AIAbility.md` + `AIAbility_00429470.c`; prior art IR L1828/L1875/L2409/L5981)

`AIAbility 0x429470` is called from `AIMainUpdate` (L216) only when session time 0x4480 > deadline
0x4404; immediately before the call the deadline is rewritten `now + 4.0` → **4 s cadence**. In
RacePhase 7 (race), with `RingBufferAccessWithWrap(0x4044)` 0/1, `this[4431] = 0x453C` is updated in
up to **two writes per call**:

1. `s4508 = 0x4508 == 0` → `v3 = max(pace - 0.01, 0x44F4)`
   `s4508 != 0`          → `v3 = min(0x44F4 + 0x44D8, pace + 0.01)`
2. if `s450c = 0x450C != 0` (random gate `(0x44D4 - s450c*0.5)*100 >= rand()%100`):
   up `v4 = min(0x44F4 + 0x44D8, v3 + 0.01)` else down `v4 = max(0x44F4*0.5, v3 - 0.01)`.

Then both accumulators are cleared. `carConditions == 1` → `pace = 0x44F4 * 0.85` (separate branch,
skips the logic above). Other writers of 0x453C (`phase2/WRITER_CENSUS.csv`):

| writer | line | write |
|---|---|---|
| `AIAbility` | L51, L68, L92 | the model above; L92 the carConditions branch |
| `AIDriverStructInit` | L55 | initialises 1.0 (bit pattern) |
| `AIPushLap` | L23-L31 | `pace = laps * 0.003 + pace` (non-race push, toward `0x44F4+0x44D8`) |
| `LateralForceTorqueCalculation` | L3 | `pace = a2 * 0x44F4 * 0.01`; called with a2=100 → baseline reset |

`0x44F4/0x44D8/0x44D4` are per-driver RaceAbility functions (`value_matches.json`):
`0x44F4 = 0.96 - 0.0096*R`, `0x44D8 = 0.0268 + 0.006432*R`, `0x44D4 = 1 - 0.030455*R`.

## 2. Live replay — Monza (5467 pace-change events, 38 car-runs incl. watcher restart overlap)

Reachable-value test: each observed change must equal one of the one-write / two-write results of the
model (or the known external writers) evaluated from the previous 1 Hz sample (accumulator ambiguity
between samples is covered by enumerating both gate states).

| matched model value | events | share |
|---|---|---|
| 1w up toward baseline+push | 2019 | 36.9 % |
| 1w down toward baseline | 1380 | 25.2 % |
| baseline reset (`LateralForceTorqueCalculation`) | 1053 | 19.3 % |
| 2w down then down(0.5·baseline) | 528 | 9.7 % |
| 2w up then up | 429 | 7.8 % |
| 2w up then down(0.5·baseline) | 28 | 0.5 % |
| 2w down then up | 26 | 0.5 % |
| cond85 `0.85*baseline` | 2 | 0.0 % |
| **UNEXPLAINED** | **2** | **0.04 %** |

- Cadence: median interval between events **4.1 s** per car (1 Hz sampling floor) — consistent with the
  4 s deadline; `0x4404 - sim_t` in [0, 4.05] in 57 033/62 510 samples (91 %; the rest are pre-race /
  stopped phases, `watch_race4fullc` is almost all pre-start).
- |step|: median 0.01000, p90 0.02000, max 0.18370; 778 events in (0.0101, 0.0201] = the random-gate
  double write; 4 events > 0.0201, all identified externally (2 × `0.85*baseline`, 2 × 1.0).
- Pace range per car 0.9503..0.9869 except two cars that hit 0.8163 (= 0.85·baseline) and 1.0;
  `ability_baseline_scalar` = 0.9603, `race_ability_push_scalar` = 0.0266 on Monza (target 0.9869).
- The 2 unexplained events (Coulthard t=3199, Räikkönen t=2496, both `watch_race4fulld`) are
  `0.8163 -> 1.0` at the exact sample where the car reaches **speed 0.00 m/s, no opponents logged, and a
  carConditions 1 → 0 transition** (stopped/spun/parked after a car-problem episode). They are a
  stop/reset path outside `AIAbility`, not a driving-pace decision (2 of 5467; Monza ran with the
  owner's SC/RF mods, which cannot be excluded as the writer).

## 3. Live replay — wet Imola (1399 events, 19 cars, 6 laps)

| matched model value | events | share |
|---|---|---|
| 1w up toward baseline+push | 547 | 39.1 % |
| baseline reset | 397 | 28.4 % |
| 2w up then down(0.5·baseline) | 301 | 21.5 % |
| 2w down then down(0.5·baseline) | 104 | 7.4 % |
| 2w up then up | 50 | 3.6 % |
| **UNEXPLAINED** | **0** | **0 %** |

- Cadence median 4.1 s; |step| median 0.00520 (target-clamped at the wet `base+push = 0.9975`),
  p90 0.01000, max 0.01520. Baseline 0.9923, push 0.0052, range 0.9823..0.9975.
- **VERDICT (model): the 4 s AIAbility model (both writes, traffic-gated) explains 5465/5467 Monza and
  1399/1399 wet events; not disproven anywhere. The two Monza outliers are a stopped-car reset.**

## 4. Rubber-band test (is 0x453C gap-driven?)

| correlation with 0x453C | Monza | wet Imola |
|---|---|---|
| gap to leader (progress-based, m) | r = 0.102 | r = 0.050 |
| nearest-opponent distance (m) | r = **-0.301** | r = -0.129 |
| race position | r = 0.175 | r = 0.069 |
| lap number | r = -0.191 | r = 0.041 |

- Traffic-drive is visible instead: median pace in traffic (<15 m) 0.9869 vs free air (>100 m) 0.9603 on
  Monza (+0.0266 = full push); wet 0.9975 vs 0.9923. Samples with `s4508 != 0` (traffic processed) have
  higher pace than `s4508 == 0` (Monza 0.9769 vs 0.9603).
- **VERDICT (rubber-band): no. 0x453C follows the per-driver traffic accumulator and the
  baseline/push targets; the weak gap/position correlations are a consequence of being in traffic, not a
  gap-based boost. Slot 0 (player) is not present in `overview.jsonl`, so the literal "gap to player"
  test could not be run — nearest-opponent distance is the proxy used.**

## 5. Sources of 0x4500 / 0x4504 / 0x4580

| field | source | writer | scope |
|---|---|---|---|
| 0x4500 `corner_speed_force_offset` | .rcd token `CorneringAdd` (`config_schema.json` L7873-7881) | `sub_424080` L63 (default 1.5f), L120 (parsed) | per-driver |
| 0x4504 `corner_speed_force_coeff` | .rcd token `CorneringMult` (L7883-7891) | `sub_424080` L64 (default 0.96499997…), L126 (parsed) | per-driver |
| 0x4580 `corner_speed_scale` | .rcd `RainAbility` (L7843) × global wetness `flt_8AFEE8` | `sub_424080` L150 and `AIPushLap` L33-37: `1.0 - wetness * rain_ability_scaled` | per-driver rain ability + global/track wetness |
| 0x453C | per-driver RaceAbility via 0x44F4/0x44D8/0x44D4 + traffic accumulators | `AIAbility` (4 s) | per-driver + traffic, not difficulty |

Defaults are difficulty-independent here; the difficulty scalar is used elsewhere (0x457C cap,
`AISteeringCorrection`). `sub_426430` L140 reads `0x4504*v + 0x4500` for the cornering coefficient, and
`AIComputePassScore` uses the same pair for the corner target (D1/D11).

## Limitations

- 1 Hz overview samples cannot resolve the order of the two writes inside one AIAbility call; the
  classification enumerates all reachable one-write/two-write values, so it verifies the value lattice and
  the direction bounds, not the write order per event.
- Accumulators 0x4508/0x450C are cleared at each call; a 1 Hz sample showing 0 does not prove the gate
  was off at the call instant (covered by enumeration).
- SC/RF mods active on Monza; no SC/RF marker in the logs. The 2 outliers are MOD-suspect.
- Player slot not logged → literal gap-to-player correlation impossible with these logs.
