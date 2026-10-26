# 41 Runbook — RaceAbility runtime verification (corpus-internal, not shared)

Resume: user restarts session in **admin shell**, says "go" → run steps below, paste
results into `docs/41-raceability-mechanism.md` §3–§4, bump status, promote
`name_index.json`, regen SDK (`tools/gen_sdk_from_index.py`).

Game must be: running, **in race** (green flag), difficulty slider noted (100% ideal).

## Steps

1. `python tools\poke.py read SessionType` → expect **7** (race).
2. `python tools\standings.py` → slot order + names (roster hardcoded to install).
3. Pick strongest (e.g. M.Schumacher) + weakest backmarker.
   Slot N → veh base = `0x5EC6D8 + N*0x48C0`. (Slot 0 = player.)
4. Read per driver (example slot 1, base `0x5F0F98`):

   | Field | Off | Slot-1 addr | Command |
   |---|---|---|---|
   | ability norm | +0x44C4 | 0x5F545C | `python tools\poke.py read 0x5F545C:float` |
   | baseline scalar | +0x44F4 | 0x5F548C | `python tools\poke.py read 0x5F548C:float` |
   | live pace factor | +0x453C | 0x5F54D4 | `python tools\poke.py read 0x5F54D4:float` |
   | global scalar | — | 0x5EA3D8 | `python tools\poke.py read 0x5EA3D8:float` |

5. Ground truth: raw `Raceability` from both drivers' `.rcd` (game install). Verify:
   - `(22 − RA)/22 ≈ 0x44C4` → scale+sign proven (DERIVED → FACT)
   - `(1 − RA/100) × [0x5EA3D8] ≈ 0x44F4` → second formula confirmed
6. Dynamics: `python tools\poke.py watch 0x5F54D4:float --hz 4` during green running
   → expect ±0.01/frame walk toward `0x44F4` floor; jumps during duels.
   Repeat for weak car.
7. Collect numbers → return to LLM.

## Gotchas

- Admin shell required (OpenProcess denied otherwise).
- Game NOT paused when reading dynamics.
- Difficulty <100 damps `0x44C4` by `(slider/100)²` — adjust expected value.

## Results (2026-07-26, race lap 53 Monza, 22 cars, admin shell)

- Difficulty slider: **92** (`AIDifficultySlider` live read, int)
- flt_5EA3D8 (global scalar): **0.96**
- Strong driver: Michael Schumacher, veh slot 1 (base 0x5F0F98): RA rcd = **1.8**,
  0x44C4 = **0.777149**, 0x44F4 = **0.94272**, 0x453C range = **0.94272–0.95272**
  (one −0.01 step seen, settles at baseline)
- Weak driver: Pedro De La Rosa, veh slot 19 (base 0x642D18): RA rcd = **10.6**,
  0x44C4 = **0.438589**, 0x44F4 = **0.85824**, 0x453C = **0.953219 stable** =
  0x44F4 + (1−0x44F4)×0.67 exactly → sitting at the attack/boost target (pushing)
- Formula checks:
  - **0x44C4 = (22−RA)/22 × (slider/100)² — PASS** on 8/8 clean drivers
    (Schumacher 1.8, R.Schumacher 4.0, Hakkinen 2.8, De La Rosa 10.6, Montoya 10.38,
    Irvine 6.2, Salo 8.2, Frentzen 5.3; residual < 0.001 in every case).
    (92/100)² = 0.8464 is the exact damping factor — quadratic slider damping
    confirmed numerically, not just sign.
  - **0x44F4 = (1−RA/100) × 0x5EA3D8 — PASS exact** (Schumacher 0.982×0.96=0.94272;
    De La Rosa 0.894×0.96=0.85824).
  - **NEW: 0x44C4 clamps at 1.0.** Alonso (slot 21) has effective RA ≈ −82.3 in
    memory → (22−RA)/22×0.8464 ≈ 4.0 unclamped, observed 0x44C4 = 1.0 exactly,
    while 0x44F4 = 1.7504 = (1+0.8233)×0.96 unclamped.
- Anomaly (scope note): Webber (slot 15) and Alonso (slot 21) run with **effective
  RA ≠ loose SeasonData 2001 rcd** (Webber ≈ 7.30 vs file 11.3; Alonso ≈ −82.3 vs
  file 12.2). Both formulas still hold with the effective value ⇒ single baked RA
  source feeds all fields; loose rcd is NOT ground truth for every driver in this
  install (randomizer present). All other 8 sampled drivers match their loose rcd.
- Verdict: §4.1 formulas CONFIRMED live; promote 0x44C4 / 0x44F4 / 0x453C / 0x5EA3D8.
