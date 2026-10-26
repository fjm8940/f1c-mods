# Car sweep - engine model vs every installed car (Claude, 2026-10-01)

Offline test: parse every engine .ini under `SeasonData\Vehicles`, derive the model car exactly as the DLL does
(`mf_car_derive`), run GENTLE / RACE / CASUAL / ABUSE profiles (3 h cap) and two manual faults.
Run from a scratch folder in `%TEMP%` (per-run output is disposable):
1. copy `parse.py`, `sweep.c`, `stats.py`, `grade_test.c` plus `dll/mf_engine_model.h`, `dll/mf_compat.h`,
   `dll/mf_spatial.h` and `tools/engine_model_test.c` there;
2. `python parse.py` -> cars.csv (with the E9q season column); `gcc -O2 -w -I. -o sweep.exe sweep.c -lm`
   (32-bit MinGW); `sweep.exe [-j N] > out.csv` (grade + season columns);
3. `gcc -O2 -w -I. -o grade_test.exe grade_test.c -lm`; `grade_test.exe [-j N]` -> per-car/season grades and the
   E9q real-data asserts (2001 order, 1995 flatness, 1994 Ford vs works, season medians);
4. `python stats.py`. Results and conclusions: `docs/CAR_SWEEP_2026-10-01.md`.

S1 (ADDENDUM 5, 2026-10-01): `-j N` runs the independent (car, profile, seed) jobs on N Win32 worker threads
(Q6: default = ALL logical processors, `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`; `-j N` overrides,
`-j 1` = the serial path). stdout is byte-identical across -j (results are
written into per-car slots and printed/merged in the original car order); stderr gets one progress line per
finished car plus the total wall time. Calibration modes keep the same flag: `sweep.exe q4 [seeds] [eta]`,
`sweep.exe q5a [seeds]`, `sweep.exe q5b <beta> <eta> <seed_lo> <seed_hi>` (the Q5 grid driver is `q5_cal.py`),
`sweep.exe q6 <variant> [seeds]` (ADDENDUM 6: AI countdown, onset_model 0; variant 1 = rate 1.0, variant 2 =
per-car Q1 rate; driver `q6_cal.py`; ADDENDUM 7 Q7: the AI early layer is OFF (Q7b) and any model terminal
counts, including Q7a's life-zero hard terminal).
Measured on the 20-thread dev box (2026-10-01): normal sweep 101 s -> 8.7 s; q4 200 seeds 465 s -> 54 s;
grade_test < 0.1 s both; normal out.csv hash D21249C0 both. Build stays `-O2` (no -march=native: float results
must not move).

B8 (ADDENDUM 14, 2026-10-02): `sweep.exe b8 [seeds]` runs the AI symptoms + driver reaction layer
(dll/mf_ai_react.h; countdown onset, fixed rate 1.0, R2 limp hands the clock to the fault physics,
mistakes can blow the engine) for the 1997 + 2005 field, T = 5400 s, 200 seeds/car (37 s on the dev box).
The driver `b8_cal.py` evaluates the section-7 fairness gates (per-season +-10 %, per-team 2SE,
2005 Spearman >= 0.7, nurse share 20-40 %, missed/false %) plus the synthetic RaceAbility terciles.
Requires dll/mf_ai_react.h next to sweep.c. Result + open calibration question:
docs/AI_REACTIONS_RUN_2026-10-02.md.
