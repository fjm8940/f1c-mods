# Claude review 8 — C6 ABI v1.1 (2026-10-02)

Verdict: **PASS.** test_abi OK (v1 61, v1.1 140). v1 byte-identical. brain_core loads v1.1.
Live sanity read (Claude, read-only, Klien slot 1): 140 fields read; lap_number 42, race_position 6, best_lap 80.83 s,
lap_distance 667 m, engine_life_remaining 30552, reason_code 0, retired_flag 0, call_to_box 0 - all plausible.
Accepted judgement calls: FACT/CONFIRMED from static sources mapped to DERIVED (conservative; the source field keeps
the original tier), unnamed_4040 and unnamed_4570 stay GUESS.
Coverage claim accepted with its stated limits: 100 % of the base-resolved AI-function vehicle offsets; 6 base-open
objects and 51 second-level callees still unknown.
Remaining: C1 live runs (Matias names situations).
