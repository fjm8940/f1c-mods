# Teammate-skill correction spec (2026-10-16, owner "do these fixes")

Problem: the rated driver is compared with the AI teammate in the same car; a weak AI teammate (high RaceAbility number)
makes the driver look better, a strong one worse. Fix: before comparing, convert each teammate lap to what the field's
median AI driver would do in that car.

## Evidence (this session)
- FACT (findings, CONFIRMED live 21/21): veh+0x44C4 = 0.8464 - 0.038473 x RaceAbility -> RA = (0.8464 - v) / 0.038473.
- MEASURED 2026-10-16 (traces pid18576 + pid15980, 2002 A-1 Ring, AI vanilla, fastest-half race laps, 15 same-team
  pairs with known RCD RaceAbility): about 0.2-0.4 s per RA point (~0.42 % of a 72 s lap); same-RA pairs still differ
  0.03-0.28 s -> noisy. Theory (1 - RA/100, exponent 1) would give ~0.7 s; NOT used.
- Constant: `SKILL_PCT_PER_RA = 0.42` (% of lap time per RaceAbility point). ESTIMATE.

## Rule (Python reference, C must match)
Lap records may carry `teammate_ra` and `ref_ra` (both numbers; absent/None/NaN = no correction).
`tm_skill = round(tm * (1 - SKILL_PCT_PER_RA / 100 * (teammate_ra - ref_ra)), 3)` applied to the teammate lap FIRST,
then the existing P3 level conversion (`teammate_at_level`) on that value (the level conversion uses the corrected
time as its ai_lap). A higher teammate_ra (weaker AI) gives a faster corrected time.
`ref_ra` = median RaceAbility of all AI cars in the sample except the rated car (DLL computes it).
Everything else (fastest_half_gap, laps_step, nudge_step, race_update) unchanged; laps without the keys behave as today.

## C
`AadLapRef` gains `double tm_ra, ref_ra` (NAN or absent = none; zero-initialised = 0.0 is a VALID RA, so the DLL
sets NAN explicitly when unknown - use aad_none()). `#define AAD_SKILL_PCT_PER_RA 0.42`. Harness: lap objects accept
`teammate_ra`, `ref_ra`. Parity: random values 0..7 incl. absent, both keys / one key only (one key only = no correction).

## DLL (Claude)
Read veh+0x44C4 per car each sample; teammate RA for the lap = RA of the teammate car (by car name, not proxy, not the
real player); ref_ra = median over AI cars except the rated car. Stored per lap in the lap ring and in the live file /
session record (`teammate_ra`, `ref_ra` per lap).

## Correction (same day, live race 3)
The RA read formula above is the slider-92 case. Game code sub_424080 lines 155-158: veh+0x44C4 = (22 - RA)/22 x (slider/100)^2 when the slider < 100. At slider 93 Raikkonen 3.35 read as 2.94. Fix handed to the DD-26 session (it owns dlldaptive_ai_difficulty.c right now); finding `rcd-raceability-0x44C4-slider-scaled-2026-10-16`.
