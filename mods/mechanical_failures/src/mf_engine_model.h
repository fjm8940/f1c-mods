/* mf_engine_model.h - realistic engine failure model v2 (E1, 2026-09-30).
 *
 * Implements the DeepSeek handoff sections 1.1-1.3 and 4.0-4.9, E1 only:
 * pure C, no windows.h, no game addresses, no globals. The caller (E2,
 * engine_failure.c) reads game memory into MfCar / MfInputs, calls the model,
 * and applies MfOut through the existing levers.
 *
 * Authority: the game's own engine life clock is the only authority on WHEN
 * (handoff 1.1). This model reads `life_s` every step, measures its drain
 * (EMA, tau 60 s; E8e: samples above MF_DRAIN_MAX_SPS or life increases are
 * reseed/jumps and are skipped), predicts `time_to_fail = life / drain`,
 * fires one onset when `time_to_fail < lead` (lead rolled once per session,
 * 240-900 s) after MF_DRAIN_WARM_S of valid on-track running samples and
 * MF_ONSET_CONFIRM_S continuously below lead (E8e), and
 * develops the rolled fault toward its terminal state at the current pace.
 * At steady pace (medium severity, unit physics) the fault reaches terminal
 * exactly when the game's own clock would run out. Gentler driving slows the
 * game clock AND this model's growth (growth is driven by
 * `clock_drive = drain_ema / life_at_onset`); physical abuse factors (rev^2,
 * oil heat, film breakdown, g-load, shifts, lean, fire hazard, over-rev)
 * accelerate it, so abuse can kill the engine before the game's clock ends.
 * When the game's clock reaches 0 first, the model marks itself culminated
 * and stops adding effects; it never writes the clock.
 *
 * All car-dependent values are derived from the car the game has loaded
 * (MfCar). Model constants are relative to the car's own values (rev limit,
 * rpm/oil base and range, optimum oil temperature); only material physics
 * carry absolute constants, each commented below.
 */
#ifndef MF_ENGINE_MODEL_H
#define MF_ENGINE_MODEL_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Constants block (handoff 4.x). Units are in every name.             */
/* ------------------------------------------------------------------ */

#define MF_MODEL_DT            0.01    /* s   fixed internal integration step (4.0) */
#define MF_RPM_TO_RADS         0.10471975511965977  /* J3: rpm -> rad/s (engine.ini
                                        *     LifetimeEngineRPM values are rpm, the
                                        *     model works in rad/s) */
#define MF_DT_MAX_S            10.0    /* s   largest call step treated as gameplay time
                                        *     (CTRL+T x30); dt <= 0 or larger is a clock
                                        *     jump: skip the step and re-base (E9o) */
#define MF_LEAD_MIN_S          240.0   /* s   onset lead window low (1.1) */
#define MF_LEAD_SPAN_S         660.0   /* s   onset lead window width (1.1) */
#define MF_MANUAL_BUDGET_S     300.0   /* s   virtual development budget for E4 manual triggers (E1b) */
#define MF_LIFE_EMA_TAU_S      60.0    /* s   drain EMA time constant (1.1) */
#define MF_DRAIN_MIN_SPS       1e-3    /* s/s drain floor used in time_to_fail (1.1) */
#define MF_ARM_DRAIN_REF       0.25    /* s/s 2026-10-05: drain floor when a virtual-budget
                                        *     arm (manual / impact / early) anchors its
                                        *     clock. Live: buttons pressed at pit-out read
                                        *     drain 0.0000 -> anchor 1e-3 -> the fault ran
                                        *     ~12-300x fast once at race pace (water 8 L
                                        *     gone in 25 s, bearings in 40 s; target 300 s).
                                        *     Race-pace drain live 0.21-0.30 (2026-09-30,
                                        *     2026-10-05). Above it the real drain is used. */
#define MF_ARM_DRIVE_MAX       1.5     /* -   2026-10-05: cap on drain / anchored drain for a
                                        *     virtual-budget arm (abuse can at most cut
                                        *     the budget by a third; harness race pace drains
                                        *     1.86 s/s, live 0.25 - one floor fits neither) */
#define MF_DRAIN_MAX_SPS       20.0    /* s/s drain sample cap: larger drop = reseed/jump (E8e) */
#define MF_DRAIN_WARM_S        60.0    /* s   valid on-track running samples before a natural onset (E8e) */
/* 2026-10-06 owner (vanilla+): automatic faults start only on a WARM engine.
 * FACT (EngineStressSimulation 0x43EF20): every session starts cold and the
 * native thermostat holds oil at OptimumOilTemp - 10 K until the water warms;
 * 'warm' = oil 0.5 K past that hold. Fallback: 300 s of on-track running, so a
 * car that never leaves the hold (cold day, big radiator) is not exempt. */
#define MF_WARM_OIL_DK         9.5     /* K   warm = oil >= OptimumOilTemp - this */
#define MF_WARM_FALLBACK_S     300.0   /* s   on-track running seconds that also count as warm */
#define MF_ONSET_CONFIRM_S     10.0    /* s   time_to_fail must stay below lead this long (E8e) */
#define MF_REV_REF             0.85    /* -   reference rev fraction (race pace, tests) */
#define MF_LOAD_REF            0.6     /* -   reference load frac (race pace, tests) */
#define MF_SEV_SMALL           0       /* -   severity codes (1.3) */
#define MF_SEV_MEDIUM          1
#define MF_SEV_BIG             2
#define MF_SEV_GAIN_0          0.6     /* -   small  severity growth gain (size 50%) */
#define MF_SEV_GAIN_1          1.0     /* -   medium severity growth gain */
#define MF_SEV_GAIN_2          1.6     /* -   big    severity growth gain (size 15%) */
#define MF_SEV_P_SMALL         0.50    /* -   severity roll: small */
#define MF_SEV_P_MEDIUM        0.35    /* -   severity roll: medium (cumulative 0.85) */
#define MF_STABILISE_P         0.15    /* -   chance a fault stops growing (1.3.7) */
#define MF_STABILISE_MIN_S     60.0    /* s   stabilise delay low */
#define MF_STABILISE_SPAN_S    240.0   /* s   stabilise delay width (60-300 s) */
#define MF_SHARE_TAU_S         600.0   /* s   stress-share window (1.2) */
/* 2026-10-09 share-units fix (tester log Tester A: every player DMG line pinned
 * at exhaust 0.230 = thermal share 100 %): each stressor is now a
 * dimensionless ratio to its reference so one cannot swamp the others. */
#define MF_SHARE_EXH_AVG_TAU_S 20.0    /* s   running mean the exhaust swing is measured from */
#define MF_SHARE_THERM_REF_K   150.0   /* K   exhaust swing that counts as 1 (= MF_PARIS_DT_REF_K) */
#define MF_SHARE_WATER_REF_K   10.0    /* K   water excess over oil_opt that counts as 1 */
#define MF_SHARE_G_REF         1.0     /* g   combined g above 2 g that counts as 1 */
#define MF_OFFTRACK_BOOST_S    30.0    /* s   radiator debris x3 window after excursion (1.2) */

/* 4.0 shared ----------------------------------------------------------- */
#define MF_AMBIENT_FALLBACK_K  298.15  /* K   fallback ambient if missing */
#define MF_POWER_MIN           0.45    /* -   power product floor while running (4.8) */
#define MF_AIR_DENSITY         1.2     /* kg/m3 air density for dynamic pressure (4.0) */
#define MF_EXH_TAU_S           2.0     /* s   exhaust gas temperature time constant (4.0) */
#define MF_EXH_RISE_K          930.0   /* K   exhaust rise at full load and full rev
                                        *     (E9b item 4; the target carries a rev
                                        *     factor 0.6 + 0.4 x rev) */
#define MF_EXH_LEAN_K          150.0   /* K   lean-mixture exhaust temperature rise (4.0) */
#define MF_MU_B_K              1200.0  /* K   Vogel viscosity B (material, absolute) */
#define MF_MU_C_K              200.0   /* K   Vogel viscosity C (material, absolute) */
#define MF_FUEL_EMA_TAU_S      2.0     /* s   fuel flow averaging (4.0) */

/* 4.0a component temperatures ------------------------------------------- */
#define MF_PIS_RISE_K          180.0   /* K   piston crown rise over oil at load*rev 1 (4.0a) */
#define MF_PIS_TAU_S           3.0     /* s   piston crown time constant */
#define MF_PIS_LIM_K           573.0   /* K   piston crown material limit (300 C, absolute) */
#define MF_EV_DROP_K           150.0   /* K   exhaust valve drop below T_exh (4.0a) */
#define MF_EV_TAU_S            2.0     /* s   exhaust valve time constant */
#define MF_EV_LIM_K            1073.0  /* K   exhaust valve material limit (800 C, absolute) */
#define MF_BRG_RISE_K          35.0    /* K   main bearing rise over oil at rev 1 unworn
                                        *     (E9f: physical value; the old 300 was the
                                        *     carrier of the E8g film calibration, which
                                        *     now lives in MF_FILM_TEMP_GAIN) */
#define MF_BRG_TAU_S           5.0     /* s   main bearing time constant */
#define MF_FILM_TEMP_GAIN      (300.0 / 35.0) /* -  film-viscosity temperature gain over the
                                        *     displayed bearing rise (E9f): the film
                                        *     viscosity is evaluated at
                                        *     oil + gain x (t_brg - oil), which reproduces
                                        *     the E8g calibration (old 300 K rise) exactly
                                        *     while T_brg itself is physical (35 K) */
#define MF_HEAD_RISE_K         40.0    /* K   head rise over water at full load (4.0a) */
#define MF_HEAD_TAU_S          8.0     /* s   head time constant */
#define MF_SETTLE_TAU          5.0     /* -   settle check: within 5 tau (4.0a test) */

/* 4.0b gear change and engine braking ------------------------------------ */
#define MF_SHIFT_JUMP          0.08    /* -   downshift rev jump > 8% of rev limit (4.0b) */
#define MF_SHIFT_WINDOW_S      0.2     /* s   jump measurement window */
#define MF_SHIFT_LOAD_MAX      0.1     /* -   downshift requires load < 0.1 */
#define MF_N_SHIFT             1e9     /* -   conrod damage at a reference 8% jump (4.0b) */
#define MF_B_SHOCK             0.02    /* -   bearing health lost per jump when film low (4.0b) */
#define MF_EB_LOAD_MAX         0.05    /* -   engine-braking load threshold (4.0b) */
#define MF_EB_REV_MIN          0.6     /* -   engine-braking rev threshold */
#define MF_EB_K                1e-4    /* 1/s valvetrain/bearing wear at rev 1 engine braking (4.0b) */

/* 4.1 valve return force (generic, era-neutral; E9c renamed the concept from
 * "pneumatic valve return"; the same math, constants and targets. R = 1 is a
 * full return force, it decays by leak + use and sets the rev ceiling) ------ */
#define MF_T_BAY_REF_K         380.0   /* K   bay temperature at which R = 1 is "full" (4.1) */
#define MF_VALVE_LEAK_A0       0.0012  /* 1/s medium leak area (normalised, 4.1) */
#define MF_VALVE_LEAK_GROW     0.0022  /* -   leak area growth per clock-drive second (4.1) */
#define MF_VALVE_USE_K         0.0006  /* 1/rev valve-event return-force use (4.1) */
#define MF_VALVE_CEIL_MIN      0.35    /* -   minimum rev ceiling (clamp, 4.1) */
#define MF_VALVE_CEIL_MAX      1.0     /* -   maximum rev ceiling (clamp, 4.1) */
/* R3a (ADDENDUM 11, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): severity floors of
 * the float ceiling (fraction of stock). The leak area stops growing and the
 * return pressure is floored once the ceiling reaches its severity floor, so
 * a valve-return fault plateaus (limp home) instead of declining to
 * MF_VALVE_CEIL_MIN forever. Driver float damage (h_vt) stays live: revving
 * above the floor still kills the valvetrain. */
#define MF_VALVE_CEIL_FLOOR_SMALL 0.90
#define MF_VALVE_CEIL_FLOOR_MED   0.75
#define MF_VALVE_CEIL_FLOOR_BIG   0.60
#define MF_VALVE_P_LOW         0.70    /* -   LOSING REVS warning threshold (4.1) */
#define MF_VALVE_P_PIT         0.55    /* -   pit-advice threshold (unused while
                                        *     MF_VALVE_PIT_TOPUP is 0) (4.1) */
#define MF_VALVE_FLOAT_K       15.0    /* 1/s valvetrain damage while floating (4.1) */
#define MF_VT_SHIFT_EARLY      0.6     /* -   SHIFT EARLIER red threshold (4.1) */

/* 4.2 lubrication --------------------------------------------------------- */
#define MF_OIL_START_L         6.0     /* L   sump oil at session start (material default, 4.2) */
#define MF_PUMP_FLOOR          0.25    /* -   pump efficiency floor (4.2) */
#define MF_PUMP_K              0.90    /* -   pump wear range 1.0 -> 0.25 (4.2) */
#define MF_OIL_A_GROW          0.0012  /* -   leak area (normalised 1 = medium) growth per clock-drive s (4.2) */
#define MF_OIL_LEAK_K          2.5     /* L   oil lost over one budget at medium (4.2) */
#define MF_OIL_SMOKE_K         0.5     /* -   smoke density per normalised leak rate (4.2) */
#define MF_OIL_HEAT_MAX_KPS    2.0     /* K/s extra oil heat at rev 1, dry sump (4.2) */
#define MF_FILM_CRIT           0.004   /* -   bearing film wear threshold, relative film (4.2; E8g
                                        *     recalibrated so the 4.2 race targets hold at both oil =
                                        *     optimum and oil = optimum - 10 K, the game thermostat) */
#define MF_BEARING_K           2.8     /* 1/s Archard wear coefficient (4.2; E8g recalibrated) */
#define MF_BEARING_HEAT_KPS    0.3     /* K/s friction heat at fully worn bearings (4.2; E8g: the old
                                        *     1.5 K/s loop-heated the oil into a runaway at oil = optimum) */
#define MF_BEARING_ROUGH       0.4     /* -   rough-running health threshold (4.2) */
#define MF_COOL_OIL_MARGIN_K   5.0     /* K   COOL THE OIL warning margin over optimum (4.2) */
#define MF_OIL_WARN_DK         8.0     /* K   2026-10-05 c: OIL TEMP also needs the car's own oil
                                        *     normal + 8 K (live Indy healthy oil 112.5 C vs
                                        *     optimum 108.6 C: opt + 5 alone fired at once) */

/* 4.3 cooling ------------------------------------------------------------- */
#define MF_WATER_START_L       8.0     /* L   coolant at session start (material default, 4.3) */
#define MF_WATER_A_GROW        0.0015  /* -   leak area (normalised 1 = medium) growth per clock-drive s (4.3) */
/* T2 (ADDENDUM 12, live all-fail 2026-10-01): the old MF_WATER_LEAK_K 1.5 was
 * multiplied by clock_drive a SECOND time on top of the area integration, so
 * the total coolant lost per budget SHRANK with the budget (8 L never left the
 * system: live at budget 600 the gasket was still at 0.92 after 400 s). The
 * leak now empties MF_WATER_LEAK_K litres over one budget at reference flow,
 * scaled by the (clock-normalised) leak area and the pressure flow - medium
 * runs dry at ~0.7 budget, big ~0.5, small ~0.85 (test_r3i_water_budget). */
#define MF_WATER_LEAK_K        6.0    /* L   emptied over one budget at reference flow (4.3) */
#define MF_WATER_P_REF_FLOW    2.5     /* -   reference sqrt((p_sys-1)/P_REF_BAR) at the cap (4.3) */
#define MF_GASKET_DRY_K        6.0     /* 1/budget head-gasket creep while the coolant sits below
                                        *     MF_WATER_SENSOR_FRAC (T2: the live game water
                                        *     temp does not follow the radiator levers, so the
                                        *     metal-heat path, not the water temp, must end it).
                                        *     Claude review 2026-10-01: x clock_drive like every
                                        *     other fault rate (T2 had an absolute 0.02/s, which
                                        *     killed long-budget AI engines before the vanilla
                                        *     WHEN: q6 variant 1 ours-not-vanilla 0.0 -> 0.8 %).
                                        *     6.0 = the T2 calibration at the 300 s test budget. */
#define MF_WATER_P_REF_BAR     0.25    /* bar system pressure above ambient at reference (4.3) */
#define MF_DEBRIS_CLEAR_P      0.006   /* 1/s debris clear probability at reference q (4.3) */
#define MF_DEBRIS_BLOCK_SMALL  0.70    /* -   radiator BLOCKAGE by severity (4.3; E8g item 4: open
                                        *     fraction 0.30/0.15/0.05, the live 50 % test barely heated) */
#define MF_DEBRIS_BLOCK_MEDIUM 0.85
#define MF_DEBRIS_BLOCK_BIG    0.95
/* R3e (ADDENDUM 11, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): owner live - at
 * speed the remaining airflow PLUS the untouched base dissipation kept the
 * water cool at 85 % block (old Hockenheim 75-80 C). Debris now also takes
 * this share out of the BASE term (0.25 = a quarter of the stock base
 * remains at full block); the airflow term still carries e->f_rad.
 * Overridable from the build command line for the before/after measurement. */
#ifndef MF_DEBRIS_BASE_KEEP
#define MF_DEBRIS_BASE_KEEP    0.25
#endif
#define MF_RAD_DEBRIS_WARN     0.85    /* -   PIT - DEBRIS warning when open < 0.85 (4.3) */
#define MF_CAP_BAR             3.5     /* bar absolute radiator cap (material, 4.3) */
#define MF_CAVITATION_L        3.0     /* L   below this the pump cavitates (capacity x0.3, 4.3) */
#define MF_CAVITATION_SCALE     0.3     /* -   coolant capacity scale when cavitating (4.3) */
#define MF_GASKET_K            398.0   /* K   head gasket creep onset (125 C, material, 4.3) */
#define MF_GASKET_RATE         0.002   /* 1/s creep at 6 K above onset, doubling per 6 K (4.3) */
/* E8g items 2-3: cooling symptoms are relative to the car's own water
 * baseline (was absolute 115/118 C, unreachable while the game holds water
 * far below them). Head gasket and boiling stay absolute (material). */
#define MF_WATER_BASE_TAU_S    120.0   /* s   water baseline EMA time constant (E8g item 2) */
#define MF_WATER_BASE_MIN_K    323.15  /* K   2026-10-05 b: floor 50 C (was 70 C; absolute MIN_K rules guard the warnings) (live F2002 runs
                                        *     77 C); learned only above it */
#define MF_WATER_WARN_DK       12.0    /* K   WATER TEMP warning above the car baseline */
#define MF_WATER_COOL_DK       18.0    /* K   COOL THE ENGINE above the car baseline */
#define MF_WATER_STEAM_DK      10.0    /* K   steam plume above the car baseline (E8g item 3) */
/* 2026-10-05 owner: "no water temp warning at 80 C". The relative rule alone
 * fired at ~80 C on a car whose water runs below the old 70 C learn floor
 * (baseline stuck at the floor). Every water symptom now ALSO needs an
 * absolute temperature (game water normal 68-80 C across cars; water is
 * heated only by the oil, which the game holds at ~95 C). */
/* 2026-10-06 owner decision (physical markers): WATER TEMP at 100 C (the
 * coolant would boil if the system lost its pressure), COOL THE ENGINE at
 * 110 C. During a cooling fault they also need the car-relative margin;
 * without a fault (bad setup, hot day) the absolute value alone warns -
 * the learned normal follows a slow overheat up. Was 90 / 95 C. */
#define MF_WATER_WARN_MIN_K    373.15  /* K   WATER TEMP never below 100 C */
#define MF_WATER_COOL_MIN_K    383.15  /* K   COOL THE ENGINE never below 110 C */
/* 2026-10-07 (VPS usage reports: healthy cars run 100.3-100.7 C, WATER TEMP lit
 * permanently with no fault): with NO cooling fault (setup / hot day) the
 * warnings start 5 K higher. The cooling-fault path keeps 100 / 110 C. */
#define MF_WATER_SETUP_WARN_K  378.15  /* K   WATER TEMP, no fault, 105 C */
#define MF_WATER_SETUP_COOL_K  388.15  /* K   COOL THE ENGINE, no fault, 115 C */
/* 2026-10-06 owner 'water simulation correct': escaping coolant only flashes
 * to visible steam above its atmospheric boiling point (100 C); below that a
 * leak is liquid. Was 88 C (unphysical). */
#define MF_WATER_STEAM_MIN_K   373.15  /* K   steam (hot coolant escaping) never below 100 C */
#define MF_WATER_SMOKE_LPS     0.03    /* L/s escape rate for the full plume density term (4.3) */
/* R3g (ADDENDUM 11, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): with the coolant
 * gone the sensor sits in steam/air (the DLL shows the erratic gauge), the
 * engine heat goes to the oil/metal (extra oil heat through the same
 * EngineSpeedHeat lever the lubrication faults use) and the gasket creeps to
 * the terminal within the budget; white steam while the engine is hot and
 * the coolant is low. */
#define MF_WATER_SENSOR_FRAC   0.20    /* -   coolant fraction where the sensor reads steam/air */
#define MF_WATER_OIL_HEAT_KPS  1.5     /* K/s extra oil heat at zero coolant */
#define MF_WATER_LOW_STEAM     (-0.40) /* -   white steam bank while the coolant is gone */

/* 4.4 piston rings --------------------------------------------------------- */
#define MF_RING_K              1.0     /* -   ring wear coefficient (4.4) */
#define MF_RING_STOP           0.15    /* -   engine stops below this seal (4.4) */
#define MF_RING_BLOWBY_LPS     0.002   /* L/s crankcase oil loss at seal 0, load 1 (4.4) */
#define MF_RING_BLUE           1.0     /* -   blue smoke at seal 0 (4.4) */

/* 4.5 exhaust crack --------------------------------------------------------- */
#define MF_CRACK_A_SMALL       0.05    /* -   crack length by severity (4.5) */
#define MF_CRACK_A_MEDIUM      0.10
#define MF_CRACK_A_BIG         0.20
#define MF_PARIS_M             3.0     /* -   Paris law exponent (material, 4.5) */
#define MF_PARIS_C             0.0015  /* -   Paris growth coefficient per cycle (4.5) */
#define MF_PARIS_DT_REF_K      150.0   /* K   reference exhaust temperature swing (4.5) */
#define MF_PARIS_MIN_SWING_K   40.0    /* K   ignore swings below this (noise, 4.5) */
#define MF_CRACK_VIB_K         1e-6    /* 1/s vibration growth per rev^2 (4.5) */
#define MF_CRACK_POP_K         0.5     /* 1/s backfire pop chance at crack 1 (4.5) */
#define MF_REAR_HEAT_MAX       50.0    /* -   rear suspension heat integral limit (4.5) */
/* R3m (ADDENDUM 11, owner live): at race pace the manual medium crack (0.10)
 * reached the rear-suspension break after ~490-540 s, longer than the 300 s
 * manual budget - R3i requires every fault to end inside its budget. The heat
 * rate carries this multiplier so medium breaks at ~250-290 s at race pace
 * (small ~530 s, big ~135 s; harness test_r3m_exhaust_budget). */
#define MF_REAR_HEAT_K         2.5

/* 4.6 fuel system ----------------------------------------------------------- */
#define MF_FUEL_CAP_FLOOR      0.35    /* -   fuel pump capacity floor (4.6) */
#define MF_FUEL_CAP_K          0.4     /* -   capacity range 1.0 -> 0.35 (4.6) */
#define MF_FUEL_CUT_MIN        0.2     /* -   lean cut lever low value (4.6) */
#define MF_FUEL_CUT_SPAN       0.6     /* -   lean cut lever span to 0.8 (4.6) */
/* R3h/R3i (ADDENDUM 11, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): the symptom
 * curve runs on the pressure fraction (fuel_cap), scaled by load - no symptom
 * >= 0.90, occasional stumble 0.90-0.80, clear top-end cutting below 0.80,
 * the native lean-cut lever from 0.60 down. At the 0.35 floor the fault is
 * not a silent hold: after MF_FP_STARVE_S at the floor the engine stops,
 * fuel-starved (fairness floor respected). */
#define MF_FP_STUMBLE           0.90
#define MF_FP_CUT_FULL          0.80
#define MF_FP_HEAVY             0.60
#define MF_FP_LOAD_MIN          0.30
#define MF_FP_STARVE_S          60.0
#define MF_FUEL_LEAK_LPS      5e-4    /* L/s fuel leak rate at medium, reference pressure (4.6) */
#define MF_FIRE_P              2e-5    /* 1/s fire hazard coefficient (material, 4.6) */
#define MF_FIRE_T_IGN_K        800.0   /* K   fuel ignition temperature (material, 4.6) */

/* 4.7 conrods and over-rev --------------------------------------------------- */
#define MF_SN_M                8.0     /* -   S-N fatigue exponent (material, 4.7) */
#define MF_SIGMA_E             1.0816  /* -   endurance limit rev_abs^2, 1.04^2 (4.7) */
#define MF_SIGMA_REF           1.0816  /* -   reference stress, = endurance limit (4.7) */
#define MF_N_REF               1e8     /* rev reference revolutions to failure (4.7) */
#define MF_FATIGUE_REV_CAP     1.25    /* -   2026-10-07: max rev_abs fed to fatigue */
#define MF_OVR_HARD_S          0.3     /* s   above the torque-zero rpm: valve/piston death (4.7; E9a) */
#define MF_OVR_VT_REV          1.06    /* -   valvetrain over-rev damage onset (4.7; E9b:
                                        *     raised 1.02 -> 1.06 so ordinary downshift
                                        *     blips to ~1.13 cost nothing) */
#define MF_OVR_VT_DWELL_S      0.1     /* s   rpm must stay above MF_OVR_VT_REV this
                                        *     long before over-rev damage or life-jump
                                        *     accounting applies (E9b; addendum 2 s7) */
#define MF_OVR_VT_K            1.0     /* 1/s valvetrain damage above MF_OVR_VT_REV (4.7) */
#define MF_OVR_VT_PER_LIFE     2.0     /* -   valvetrain health lost per life-second of an over-rev
                                        *     life jump, rods half (E9a, relative to remaining life) */
#define MF_OVR_BLAME_S         5.0     /* s   a clock-out within this of an over-rev jump is presented
                                        *     as the valve/piston blow-up (E9a) */
#define MF_TQ_ZERO_FALLBACK    1.15    /* -   no RPMTorque table: torque-zero = this x rev limit (E9a) */
#define MF_TQ_PEAK_FALLBACK    0.85    /* -   no RPMTorque table: peak torque = this x rev limit (E9a) */
#define MF_REARM_PREV_S        5.0     /* s   native stage-one re-arm: previous life below this (E9a) */

/* 4.8d E9b: fun and fairness (paper E9b items 1-5, addendum 2 section 7) -- */
#define MF_ALWAYS_ON_FLOOR     0.5     /* -   no-onset floor for h_vt/h_b/h_rod (E9b item 2;
                                        *     also covers the E9a over-rev jump damage) */
#define MF_FAIR_FRAC           0.6     /* -   fairness floor: a model terminal may not fire
                                        *     before onset + this x time-to-fail at onset
                                        *     (owner: abuse kills at most 40 % early) */
#define MF_FAIR_HOLD           0.02    /* -   health a failing component is held at until
                                        *     the fairness floor passes */
#define MF_NIGGLE_PER_H        1.0     /* 1/h harmless-niggle rate, no onset (E9b item 3) */
#define MF_NIGGLE_MAX          3       /* -   max niggles per session */
#define MF_NIGGLE_MISFIRE      1       /* -   niggle kind: E9d rhythmic misfire */
#define MF_NIGGLE_SENSOR       2       /* -   niggle kind: dash sensor fault (HUD) */
#define MF_NIGGLE_PUFF         3       /* -   niggle kind: harmless smoke puff */
#define MF_NIGGLE_MISFIRE_D    0.3     /* -   MISFIRE niggle depth (E9d pattern) */
#define MF_NIGGLE_MISFIRE_MIN_S 2.0    /* s   MISFIRE niggle duration low */
#define MF_NIGGLE_MISFIRE_MAX_S 5.0    /* s   MISFIRE niggle duration high */
#define MF_NIGGLE_SENSOR_MIN_S 5.0     /* s   SENSOR niggle duration low */
#define MF_NIGGLE_SENSOR_MAX_S 15.0    /* s   SENSOR niggle duration high */
#define MF_NIGGLE_PUFF_BANK   (-0.1)   /* -   PUFF bank-1 value (the DLL maps it) */
#define MF_NIGGLE_PUFF_S       1.0     /* s   PUFF duration */

/* 4.8e E9b-2: early failures, cascades, engine quality, felt misfire
 * (addendum 3 items 6-10) ---------------------------------------------- */
#define MF_EARLY_PER_H         0.02    /* 1/h  early random failure rate at nominal use
                                        *      (drain 1 s/s: 2 % per on-track hour) */
#define MF_EARLY_ABUSE_MAX     5.0     /* -    drain multiplier cap in the early hazard */
#define MF_EARLY_BUDGET_MIN_S  300.0   /* s    early-failure development budget low */
#define MF_EARLY_BUDGET_MAX_S  900.0   /* s    early-failure development budget high */
#define MF_CASCADE_OIL_FRAC    0.6     /* -    oil-leak cascade: sump fraction threshold */
#define MF_CASCADE_WEAR_K      0.5     /* -    oil-leak cascade bearing wear vs the
                                        *      oil-pressure fault (half rate) */
#define MF_CASCADE_GASKET_K    (0.5 / 120.0)  /* 1/s  cooling cascade gasket health drop
                                        *      at the threshold (reaches ~0.5 after
                                        *      120 s at that temperature) */
#define MF_QUALITY_LO          0.85    /* -    engine quality triangular low */
#define MF_QUALITY_HI          1.15    /* -    engine quality triangular high */
#define MF_QUALITY_MODE        1.0     /* -    engine quality triangular mode */
#define MF_BUDGET_GROWTH_K     1.387   /* -    manual/virtual budget growth scaling for the
                                        *      oil-pressure / valve-return / blow-by arms.
                                        *      Owner live 2026-09-30: a manual medium
                                        *      (budget 300 s) oil pressure blew at ~416 s
                                        *      of race driving; 416/300 = 1.387, so the
                                        *      medium budget develops at ~the budget
                                        *      under race driving. Natural (clock-anchored)
                                        *      onsets are never scaled. */
#define MF_MISFIRE_EVT_HZ_MAX  2.0     /* 1/s  discrete misfire-cut rate at depth 0.6
                                        *      (addendum 3 section 8: 2/s keeps the
                                        *      40-120 ms cuts under the 15 % bound) */
#define MF_MISFIRE_CUT_MIN_S   0.04    /* s    discrete misfire cut length low */
#define MF_MISFIRE_CUT_MAX_S   0.12    /* s    discrete misfire cut length high */
#define MF_MISFIRE_CUT_REV_MIN 0.30    /* -    no discrete cuts below this rev fraction */
#define MF_MISFIRE_CUT_DEPTH_MIN 0.1   /* -    no discrete cuts below this depth */
#define MF_MISFIRE_CUT_GAIN    1.5     /* -   cut depth factor: strength = min(1, this
                                        *      x depth), applied as output x (1 - strength) */

/* 4.8h E9q: engine grade from the modder's own reliability data ------------ */
#define MF_GRADE_MIN           0.80    /* -   clamp low  (grade never below) */
#define MF_GRADE_MAX           1.20    /* -   clamp high (grade never above) */
#define MF_GRADE_FIELD_MAX     64      /* -   largest session field */
#define MF_GRADE_EP_FLOOR      0.25    /* -   Ep floor as a fraction of E */

/* 4.8i E9i: "save the engine" mode (paper E9i; HOW only) ------------------- */
#define MF_SAVE_REV_MULT       0.97    /* -   rev limit multiplier while saving (DLL
                                        *     band; combined with a valve ceiling,
                                        *     lower wins) */
#define MF_SAVE_GROWTH         0.6     /* -   fault growth multiplier */
#define MF_SAVE_EARLY          0.5     /* -   early-failure hazard multiplier */

/* 4.8j E9 hardening H2: emergency onset + niggle symptom (Codex R04/R05/R14,
 * D2/D3/D5) --------------------------------------------------------------- */
#define MF_EMERGENCY_MISFIRE_D 0.3     /* -   immediate felt misfire depth of an
                                        *     emergency onset (H2b; never radio
                                        *     first on a seconds-long budget) */
#define MF_EMERGENCY_POWER     0.9     /* -   immediate power cap of an emergency
                                        *     onset (felt symptom, one channel) */

/* 4.8f E9h: contact faults from wall hits (paper E9h) --------------------- */
#define MF_IMPACT_G            8.0     /* g   impact deceleration threshold */
#define MF_IMPACT_MAX_DT_S     0.25    /* s   the speed-drop window is one pump tick;
                                        *     larger steps (CTRL+T) use the code-4 edge */
#define MF_IMPACT_SEV_SMALL_G  15.0    /* g   severity boundary small/medium */
#define MF_IMPACT_SEV_BIG_G    25.0    /* g   severity boundary medium/big */
#define MF_IMPACT_P_WATER      0.40    /* -   radiator puncture -> water leak */
#define MF_IMPACT_P_EXHAUST    0.25    /* -   exhaust crack (cumulative 0.65) */
#define MF_IMPACT_P_OIL        0.20    /* -   oil leak (cumulative 0.85; 0.15 nothing) */
#define MF_IMPACT_BUDGET_MIN_S 240.0   /* s   impact-fault budget floor */
#define MF_IMPACT_BUDGET_MAX_S 600.0   /* s   impact-fault budget cap (min(ttf, cap)) */
/* 2026-10-05: impulse source (player). The DLL hands the game's own contact
 * impulse (|a4| x damage% / 100 at 0x4EC1A0, the value compared with
 * damage.ini [PHYSICAL] Engine=) once per contact burst. Live calibration
 * 2026-10-05 (F2002, 6 hits): side brush 50 km/h 1433, nose 30 km/h 1821,
 * sidepod + wing off 5564, nose 60 km/h 7732, nose 100-110 km/h 8308/8734
 * (the per-hit impulse flattens above ~60 km/h). Replaces the g check for
 * the player: that one read the pit-return teleport as a 20-49 g crash. */
#define MF_IMPACT_IMP_MIN      2000.0  /* -   below: no engine fault (taps, brushes) */
#define MF_IMPACT_IMP_MID      5000.0  /* -   real crash; severity small/medium */
#define MF_IMPACT_IMP_BIG      8000.0  /* -   big crash; severity medium/big */
#define MF_IMPACT_IMP_TOP      10000.0 /* -   damage.ini Engine= (native seizure) */
#define MF_IMPACT_PC_MID       0.25    /* -   fault chance at IMP_MID (0 at IMP_MIN) */
#define MF_IMPACT_PC_BIG       0.70    /* -   fault chance at IMP_BIG */
#define MF_IMPACT_PC_TOP       0.85    /* -   fault chance at IMP_TOP and above */

/* 4.8g E9p: AI shadow engines and the native-law TEST mode --------------- */
#define MF_AI_MAX              24      /* -  AI slots (vehicle array stride 0x48C0;
                                        *    slot 0 is the player) */
#define MF_AI_POWER_TABLE      1       /* -  1 = scale the AI car's own torque
                                        *    table power column (FACT: per-car table);
                                        *    0 = the maxSpeed fallback write */
/* P2 (ADDENDUM 2, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt; plan
 * docs/FAILURE_OCCURRENCE_HAZARD_PLAN_2026-10-01.md section 2): Weibull
 * hazard in x = consumed/budget, SHADOW ONLY (nothing wires it into onset;
 * P3 is an owner decision). P2 c calibration (`sweep.exe p2`; rows = 1997 +
 * 2005RH x 4 profiles, T = 5400 s; target = 35 model terminals within T;
 * picked the strongest eta with shadow-or-onset count <= 1.1 x target):
 *   beta 3: eta 0.607 -> 24 shadow fires, mean onset t 987 s, sd 482
 *   beta 5: eta 0.653 -> 24 fires, mean 1029 s, sd 691   <- default here
 *   beta 8: eta 0.699 -> 24 fires, mean 1138 s, sd 995
 * Shadow fault shares at beta 5: valve 11, oilleak 2, rad 2, fuelleak 3,
 * blowby 3, exhaust 3; the countdown onset drew oil pressure 35/35 (same
 * runs) - the accumulated history spreads the fault mix.
 * Q4 (ADDENDUM 3): wired live in Q3; the PLAYER keeps 0.653 (Q4c: field
 * total identical to the 911b619d countdown model at every profile, ratio
 * 1.000). The AI needs its own eta (field-average drain 1.0 s/s): with 0.653
 * the AI count overshoots. Q4b calibration (beta 5 fixed, T = 5400 s,
 * timer Normal(3Avg,3Var)/f, f = 1.0, 200 seeds/car, 1997 + 2005 folders =
 * 22 engines): vanilla Phi sum 0.6119 vs ours 0.6600 at eta 0.82 (+7.9 %,
 * inside +-10 %). Per season (report only, one eta cannot equalise both):
 * 1997 vanilla 0.1009 / ours 0.2200; 2005 vanilla 0.5110 / ours 0.4400. */
#define MF_HAZARD_BETA         5.0
#define MF_HAZARD_ETA          0.653
/* 2026-10-09 owner decision (tester log Tester A): keep the player hazard as it
 * is and give the AI the SAME curve - failures may come well before the
 * vanilla life end for both. Was 0.82 (Q4b vanilla-matched AI count); the
 * AI onset count now rises above the vanilla Phi total by design. */
#define MF_HAZARD_ETA_AI       MF_HAZARD_ETA
/* MF 1.0.3 N3 (owner D3/D6): the AI onset mass BLOW-UP takes (the other
 * faults keep their relative weights, scaled by 1 - this); and the range of
 * the rolled rough-running warning before the engine blows. The player's own
 * onset rolls never pick BLOW-UP (its weight is 0 in mf_fault_weights). */
#define MF_AI_BLOWUP_WEIGHT    0.08
#define MF_AI_BLOWUP_WARN_MAX_S 8.0
#define MF_AI_LAW_FUEL_LPS     0.005   /* L/s combustion proxy at full throttle in the
                                        *    native-law ODE (the game's per-call fuel
                                        *    term units are not fully established;
                                        *    see the E9p log entry) */
#define MF_AI_LAW_THERM_K      0.05    /* 1/s native-law thermostat pull toward the car's
                                        *    optimum - 10 K, TWO-SIDED (J2b, ADDENDUM 1:
                                        *    tau 20 s). J2b K sweep, 1800 s, car-relative
                                        *    defaults, ambient 25 C, 0.05 s substeps
                                        *    (RACE oil / SLOW-HOT oil / COLD reach
                                        *    opt-12 K): 1.0 = 371.1/371.3/3 s FAIL
                                        *    (SLOW-HOT < RACE + 3 K); 0.5 = 372.2/372.6/6 s
                                        *    FAIL; 0.25 = 374.5/375.3/10 s FAIL; 0.1 =
                                        *    379.8/382.2/18 s FAIL (by 0.6 K); 0.05 =
                                        *    382.7/393.5/27 s PASS, all water/oil inside
                                        *    273-450 K at every substep. The weakest
                                        *    passing K lets condition heat differences show
                                        *    while staying bounded (the J2 one-sided pull
                                        *    had no equilibrium and ran away). */

/* Fault codes: stable order, mirrored by the E4 control block. */
#define MF_FAULT_NONE          0
#define MF_FAULT_VALVE_AIR     1
#define MF_FAULT_OIL_PRESSURE  2
#define MF_FAULT_OIL_LEAK      3
#define MF_FAULT_WATER_LEAK    4
#define MF_FAULT_RAD_DEBRIS    5
#define MF_FAULT_FUEL_PRESSURE 6
#define MF_FAULT_FUEL_LEAK     7
#define MF_FAULT_BLOWBY        8
#define MF_FAULT_EXHAUST       9
#define MF_FAULT_BLOWUP       10   /* MF 1.0.3 N3: AI-only sudden engine blow-up */
#define MF_FAULT_COUNT        11

/* 4.8b E9e: car data + torque-curve power shaping (addendum 2 section 3) --- */
#define MF_INERTIA_FALLBACK    0.05    /* kg m2 EngineInertia fallback (E9e) */
#define MF_INERTIA_REF         0.05    /* kg m2 reference: misfire dips scale
                                        * 0.05 / inertia, clamped below (E9e) */
#define MF_MISFIRE_DIP_MIN     0.5     /* -   inertia scaling clamp floor (E9e) */
#define MF_MISFIRE_DIP_MAX     2.0     /* -   inertia scaling clamp ceiling (E9e) */
#define MF_MISFIRE_MAX         0.6     /* -   MfOut.misfire_depth ceiling (E9d) */

/* 4.8c E9d: the game's own symptoms ---------------------------------------- */
#define MF_FRICTION_K          1.5     /* -   ENG_BRAKE_MAP raise at fully worn
                                        *     bearings: 1 + K x (1 - h_b) */
#define MF_BLOWBY_BURN_K       0.03    /* -   extra fuel burn per (1 - ring_s) (E9d) */
#define MF_LEAN_BURN_K         0.05    /* -   extra fuel burn per lean shortfall (E9d) */

/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t s;                    /* xorshift32 state */
} MfRng;

typedef struct {
    double rev_limit_rads;         /* rad/s car rev limit */
    double rpm_base_rads;          /* LifetimeEngineRPM base */
    double rpm_range_rads;         /* LifetimeEngineRPM range (halving) */
    double oil_base_k;             /* LifetimeOilTemp base */
    double oil_range_k;            /* LifetimeOilTemp range (halving) */
    double oil_opt_k;              /* OptimumOilTemp threshold */
    double w_oil_ref;              /* 2^((oil_opt - oil_base)/oil_range), reference oil curve */
    double tq_zero_rads;           /* E9a: rpm of the car's torque zero (RPMTorque table) */
    double peak_tq_rads;           /* E9a: rpm of peak torque (RPMTorque table) */
    /* E9e (addendum 2 section 2/3): more engine.ini values from memory */
    double inertia_kgm2;           /* EngineInertia, kg m2 */
    double idle_lo_rads;           /* IdleRPMLogic low, rad/s */
    double idle_hi_rads;           /* IdleRPMLogic high, rad/s */
    double fuel_cons;              /* FuelConsumption, 0 = missing (fallback) */
    double fuel_mult;              /* FuelConsumption multiplier */
    double owt_k0;                 /* OilWaterHeatTransfer base, 0 = missing */
    double owt_k1;                 /* OilWaterHeatTransfer per rad/s */
    unsigned def_flags;            /* bit i = field i replaced by its relative fallback
                                    * (6/7 = tq; E9e: 8 inertia, 9 idle_lo, 10 idle_hi,
                                    * 11 fuel_cons, 12 fuel_mult, 13 owt_k0, 14 owt_k1) */
} MfCar;

typedef struct {
    double dt_s;                   /* wall time since previous call */
    double life_s;                 /* game engine life clock (READ ONLY) */
    double rpm_rads;               /* engine speed */
    double oil_k;                  /* oil temperature */
    double water_k;                /* water temperature */
    double load;                   /* 0..1 throttle/torque fraction (0x7C7430) */
    double speed_mps;              /* vehicle speed */
    double fuel_l;                 /* fuel remaining, L (refuel jumps ignored) */
    double g_long;                 /* longitudinal g (fallback: caller derives) */
    double g_lat;                  /* lateral g (fallback: 0) */
    double ambient_c;              /* ambient air, C */
    double stress;                 /* game stress term, folded into the 1.1 law fallback */
    int    engine_running;         /* 1 = ignition on and engine turning */
    int    on_track;               /* 1 = player on track (practice and pit-out included) */
    int    in_pit;                 /* 1 = PIT_STATUS 3 (in box) */
    int    off_track;              /* 1 = off the racing surface (optional surface flag) */
    int    life_infinite;          /* 1 = Failure Rate "none"/infinite: no onset */
    int    self_seize;             /* E9g-e: the DLL holds this engine dead (our own
                                    * seizure) - a zeroed clock while set is the known
                                    * terminal, not a stint reset */
    int    reason_code;            /* E9g-e: game ReasonCode (veh+0x4068); 4 = collision -
                                    * a drop to <= 0 under it is a failure, not a reset */
    int    niggles_enable;         /* E9b item 3: control-block niggle_enable (default 1) */
    int    early_enable;           /* E9b-2 item 6: early random failures enabled.
                                    *     1 in the game (always on); the model test
                                    *     harness gates it like niggles_enable so the
                                    *     calibrated RNG sequences stay untouched. */
    int    ai_mode;                /* E9p: AI shadow engine - the E9a torque-zero
                                    *     death is disabled (AI rpm is clamped) */
    double life_drain_sps;         /* E9p: >0 = the caller knows the clock's rate
                                    *     (the AI timer ticks 1.0 per game second);
                                    *     the drain EMA is set to it instead of
                                    *     re-measuring the life slope. 0 = measure. */
    int    save_mode;              /* E9i: "save the engine" on (fault growth x
                                    *     MF_SAVE_GROWTH, early hazard x MF_SAVE_EARLY;
                                    *     the rev multiplier is the DLL's band) */
    int    onset_model;            /* Q3 (ADDENDUM 3): 1 = hazard onset (default;
                                    *     0 = countdown only, the pre-P3 behaviour,
                                    *     byte-identical for A/B). Q7a (ADDENDUM 7):
                                    *     with ai_mode 1 + countdown 0 the life
                                    *     clock itself is the hard terminal */
    int    ai_reaction;            /* B5 (ADDENDUM 14): the AI reaction layer is
                                    *     live (symptoms + nursing + mistakes).
                                    *     1 = the AI fairness override is OFF (the
                                    *     physics may terminate inside the fair
                                    *     window) and Q7a's save rules apply;
                                    *     0 = the legacy byte-identical path. */
    int    ai_nursed;              /* B6 (ADDENDUM 14): the driver has noticed
                                    *     and is nursing (R1+). The AI countdown
                                    *     Q7a cap/hard terminal is skipped so the
                                    *     fault's own physics decides (a nursed
                                    *     car can reach the flag); an un-nursed
                                    *     car keeps the exact vanilla WHEN. */
    int    impulse_src;            /* 2026-10-05: 1 = impacts come from `impulse`
                                    *     (the game's contact impulse, player);
                                    *     the speed-drop g check is off.
                                    *     0 = legacy g check (AI, tests). */
    double impulse;                /* peak contact impulse of a burst that
                                    *     ended since the last step; 0 = none */
} MfInputs;

typedef struct {
    double power_mult;             /* product of all power factors, floor MF_POWER_MIN (4.8) */
    double vfuel_press;            /* fuelcut_hook lever, -1 = leave alone (4.6) */
    double vfuel_valve;            /* valve-return misfire lever, -1 = leave alone (4.1) */
    double rev_ceiling;            /* rev limit ceiling, -1 = restore (4.1) */
    double smoke_bank1;            /* 0 or -0.1..-0.6 (particle bank 1) (4.2/4.3) */
    double smoke_blue;             /* 0..1 blue smoke density (4.4) */
    double oil_heat_ks;            /* extra oil heat, K/s, through the existing write (4.2) */
    double rad_scale;              /* coolant-loss scale: ALL radiator coefficients (4.3) */
    double rad_air_scale;          /* airflow scale: RAD_AIR_K0/K1 only, 1 = healthy (E8g item 4) */
    double pop_chance_s;           /* backfire pop chance per second (4.5) */
    /* E9e: torque-curve power shaping (addendum 2 section 3 item 7): the
     * fuelcut hook applies 1 - loss_all - loss_top x s(rpm) - loss_load x
     * throttle, clamped [MF_POWER_MIN, 1]; s = smoothstep(peak, limit, rpm) */
    double loss_top;               /* valvetrain / valve return, above peak rpm */
    double loss_all;               /* bearings, every rpm */
    double loss_load;              /* rings / blow-by, throttle-proportional */
    /* E9d: the game's own symptoms */
    double misfire_depth;          /* 0..0.6 rhythmic misfire depth (fuelcut hook) */
    double oil_gauge_scale;        /* p_oil / p_oil_healthy, 1 = healthy (gauge) */
    double fuel_extra_lps;         /* extra burn, L/s (blow-by + lean, FUEL_NOW) */
    int    blow_up;                /* terminal with the loud bang (4.8) */
    int    engine_stop;            /* terminal without bang (rings/gasket, 4.8) */
    int    break_rear_suspension;  /* crack heat reached its limit (4.5) */
    char   reason[48];             /* terminal reason text (4.9) */
    int    warn_valve_air_low;     /* 4.1 warnings / instructions */
    int    warn_revs_down;
    int    warn_pit_air;
    int    warn_valve_float;
    int    warn_shift_earlier;
    int    warn_oil_pressure;
    int    warn_cool_oil;
    int    warn_water_temp;
    int    warn_cool_engine;
    int    warn_coolant_low;       /* L2 2026-10-05: low-coolant diagnosis */
    int    warn_pit_debris;
    /* E9b item 3: harmless niggles (fun, never a failure) */
    int    niggle_kind;            /* 0 none, see MF_NIGGLE_* */
    double niggle_left_s;          /* remaining niggle time, s */
    /* E9b-2: event flags for the DLL log (one step) */
    int    early_fired;            /* 1 = an early random failure just started */
    int    cascade_kind;           /* 0 none, 1 oil leak -> bearings, 2 cooling -> gasket */
    /* E9h: an impact was detected this step (the DLL logs it) */
    int    impact_fired;           /* 1 = impact detected (any outcome) */
    double impact_g;               /* its deceleration, g (at least MF_IMPACT_G) */
    int    impact_fault;           /* rolled fault, 0 = nothing / native seizure */
    double impact_impulse;         /* impulse source: the hit's impulse (else -1) */
    double impact_chance;          /* impulse source: the fault chance rolled */
    /* E9-H2e: 1 = the misfire depth comes from worn components and may run the
     * discrete cuts; 0 = a harmless-niggle pattern only (never discrete). */
    int    misfire_discrete;
} MfOut;

typedef struct {
    /* session / onset */
    MfRng  rng;
    double t_s;                    /* session age of this engine */
    double acc_s;                  /* model step accumulator */
    double life_prev_s;
    int    life_valid;
    int    eng_prev;               /* E9g-c: engine_running on the previous sample */
    unsigned life_resets;          /* E9g-c: clock resets to 0 that are not failures */
    int    life_reset_pending;     /* E9g-d: a reset was counted since the last
                                    * positive sample - no stage one after it */
    double drain_ema_sps;
    int    drain_valid;
    double drain_game_prev_sps;    /* E9g-g: game-law drain of the previous pump
                                    * sample (mf_game_drain at its rpm/oil); the
                                    * reset floor uses max(ema, this) so steep
                                    * life curves do not lag the EMA */
    double drain_short_sps;        /* E9-H2b: short-horizon raw drain from the
                                    * adjacent valid samples (> the EMA cap) */
    int    drain_short_valid;      /* E9-H2b: the short-horizon estimate is usable */
    int    drain_short_run;        /* E9-H2b: consecutive accepted short samples
                                    * (a single blip is not imminent death) */
    int    emergency_onset;        /* E9-H2b: the onset bypassed warm/confirm */
    double drain_warm_s;           /* E8e: valid, on-track, running sample seconds */
    double onset_confirm_s;        /* E8e: time_to_fail continuously below lead */
    double lead_s;                 /* rolled once per engine (1.1) */
    int    severity;               /* MF_SEV_* */
    int    fault;                  /* MF_FAULT_* */
    int    onset;                  /* 1 after the one onset fired (1.1) */
    int    onset_hazard;           /* Q3: the onset came from the hazard path
                                    * (0 = countdown backstop / emergency) */
    int    stabilised;             /* fault stopped growing (1.3.7) */
    int    culminated;             /* game clock reached 0: stop effects (1.1) */
    double blowup_warn_s;          /* N3: BLOW-UP rough-running warning left, s */
    int    blown;                  /* blow-up latched */
    int    stopped;                /* engine stopped without bang latched */
    int    rear_broken;            /* rear suspension break latched */
    double onset_t_s;
    double stop_delay_s;           /* stabilise delay rolled at onset */
    double life_onset_s;
    double drain_onset_sps;
    int    arm_virtual;            /* 2026-10-05: 1 = virtual-budget arm (manual /
                                    *     impact / early): pace ratio capped, clock
                                    *     re-anchored once the drain EMA settles */
    int    arm_reanchor;           /* 1 = re-anchor still pending */
    double arm_budget_s, arm_gain; /* the arm's budget and budget gain */
    double ttf_onset_s;            /* E9b item 1: time_to_fail at onset (fairness floor) */
    /* stress shares over MF_SHARE_TAU_S (1.2) */
    double share_rev;
    double share_oil;
    double share_water;
    double share_thermal;
    double share_g;
    double t_exh_avg_k;            /* 2026-10-09: running mean for the thermal
                                    *     share (0 = not seeded yet) */
    double offtrack_timer_s;
    /* shared derived (4.0 / 4.0a) */
    double t_exh_k;
    double t_exh_dir;
    double t_exh_peak_k;
    double t_exh_trough_k;
    double t_pis_k;
    double t_ev_k;
    double t_brg_k;
    double t_head_k;
    double fuel_prev_l;
    double fuel_flow_lps;
    double fuel_flow_peak_lps;
    double lean_shortfall;
    double shift_rpm[32];          /* E9o: pump-sample ring for the 0.2 s downshift window */
    double shift_t_s[32];          /* E9o: model time of each ring sample */
    int    shift_idx;
    int    shift_n;
    double overrev_timer_s;
    int    overrev_prev_above;     /* E9o: the previous pump sample was above tq_zero */
    double overrev_life_s;         /* E9a: life lost to over-rev life jumps */
    double overrev_last_t_s;       /* E9a: model time of the last over-rev jump */
    int    overrev_events;         /* E9a: jumps > 1 s of life */
    double ovr_vt_timer_s;         /* E9b: seconds above MF_OVR_VT_REV (dwell) */
    int    culm_overrev;           /* E9a: the clock-out is blamed on an over-rev */
    int    culm_collision;         /* E9g-e: the clock-out is the native wall-crash
                                    * seizure (ReasonCode 4): failure, not a reset */
    int    native_stage1;          /* E9a: the game re-armed its own stage one */
    double repair_grace_s;         /* N5 2026-10-06 (owner-approved): post-REPAIR
                                    * grace, game seconds while on track with NO
                                    * automatic onset of any kind; the DLL sets
                                    * 60.0 on REPAIR, 0 = off. Manual arms
                                    * (mf_fault_arm) are unaffected. */
    double wear_active_s;          /* oil warning: wear active for 3 s (4.2) */
    /* E9b item 3: harmless niggles */
    int    niggle_kind;            /* active niggle kind, 0 = none */
    double niggle_left_s;          /* remaining niggle time, s */
    double niggle_acc_s;           /* whole-second hazard accumulator */
    int    niggle_count;           /* niggles fired this session */
    /* E9b-2 item 8: engine quality (1 = nominal). Rolled once per engine from a
     * copy of the rng state, so the main draw sequence is untouched. Effects:
     * fault growth x 1/q, early hazard x (1/q)^2, niggle rate x 1/q. */
    double quality;
    /* E9q: engine grade from the car's own LifetimeAvg/Var relative to the
     * session field (1 = the field median). Effects (HOW only): fault growth
     * x G^-0.5, early hazard x G^-2, niggle rate x 1/G; always combined
     * multiplicatively with quality. Default 1.0 (field of one / missing). */
    double grade;
    /* E9b-2 item 6: early random failure (bathtub) - one per engine until the
     * model is reset (repair / session change). */
    int    early_done;             /* latched once an early failure fired */
    double early_acc_s;            /* whole-second hazard accumulator */
    /* E9h contact faults */
    double impact_prev_speed;      /* previous pump-sample speed */
    int    impact_prev_valid;
    int    impact_prev_code4;      /* previous sample had ReasonCode 4 */
    int    impact_latch;           /* one roll per crash until it goes quiet */
    /* components (4.1-4.7) */
    double valve_m;                /* valve return force R, 1 = full (E9c: was
                                    *     described as bottle gas; same math) */
    double valve_a;                /* leak area (normalised) */
    double h_vt;                   /* valvetrain health */
    double pump_eta;               /* oil pump efficiency */
    double v_oil_l;
    double oil_a;
    double h_b;                    /* bearing health */
    double film_last;              /* last computed bearing film */
    double f_rad;                  /* radiator open fraction */
    double v_w_l;
    double water_a;
    double water_base_k;           /* E8g item 2: car water baseline EMA, 0 = not yet known */
    double oil_base_k;             /* 2026-10-05 c: car oil normal (same rule), 0 = unknown */
    double h_g;                    /* head gasket health */
    double debris_clear_acc_s;
    double ring_s;                 /* ring seal, 1 good */
    double crack_a;                /* exhaust crack length */
    double rear_heat;
    double fuel_cap;
    double h_rod;                  /* conrod health */
    double h_rod_dmg;              /* Miner damage accumulator */
    double fuel_lost_l;            /* fuel lost through the leak, L (4.6) */
    double fire_acc_s;             /* whole-second fire rolls (4.6) */
    double fp_stumble_acc_s;       /* R3h: whole-second stumble rolls */
    double fp_stumble_s;           /* R3h: seconds left of the active stumble */
    double fp_stumble_d;           /* R3h: depth of the active stumble */
    double fp_floor_s;             /* R3i: seconds held on the capacity floor */
    /* P1 (ADDENDUM 2, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): component damage
     * accumulators. Every accepted life second is split across the faults by
     * the SAME stressor weights mf_roll_fault draws from; exact bookkeeping:
     * sum(dmg) == dmg_total. */
    double dmg[MF_FAULT_COUNT];
    double dmg_total;
    /* P2: hazard-onset SHADOW state (no behaviour change). hz_rng is a
     * separate stream - the main draw sequence (lead/fault/severity/...) is
     * untouched. budget_s = the first valid life sample = the engine's life
     * at model start (player ENG_LIFE; AI timer/divisor). */
    MfRng  hz_rng;
    double budget_s;
    double hz_x;               /* x = consumed/budget at the previous step */
    double hz_beta;            /* P2 calibration: defaults from the macros */
    double hz_eta;
    double shadow_fire_t;      /* t_s of the shadow fire, -1 before */
    double shadow_x;           /* x at the shadow fire */
    int    shadow_fault;       /* fault drawn from the dmg shares */
    int    shadow_fired;       /* latch */
} MfEngine;

/* ------------------------------------------------------------------ */
/* Small helpers                                                      */
/* ------------------------------------------------------------------ */

static inline double mf_clamp(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ---- R3k (ADDENDUM 11, owner live 2026-10-01): blue-smoke drive ----------
 * The blow-by / oil-leak blue plume must react to how the engine is used -
 * it was a constant same-size trail. Emission density:
 *     drive = base x (MIN + (1-MIN) x (LOAD_W x load + RPM_W x rpm_frac))
 *             + base x PUFF_GAIN x puff
 * base = 1 - ring_s (blow-by) or the oil fraction lost (oil leak); load and
 * rpm_frac come from the model inputs; puff is a 0..1 envelope set by a
 * throttle lift-off or reapply (manifold vacuum pulls oil past the rings) and
 * decays with MF_SMOKE_PUFF_TAU_S (1-2 s), and by a hard braking/cornering
 * slosh event. Emission rate = drive x MF_SMOKE_PARTS_S particles/s. */
#define MF_SMOKE_LOAD_MIN    0.12   /* - thin trail at closed throttle idle */
#define MF_SMOKE_LOAD_W      0.55
#define MF_SMOKE_RPM_W       0.45
#define MF_SMOKE_PUFF_TAU_S  1.2    /* s lift/reapply puff decay */
#define MF_SMOKE_PUFF_GAIN   1.5    /* - puff adds up to 1.5x the load factor */
#define MF_SMOKE_MIN_DRIVE   0.03   /* - below this: no particles at all */
#define MF_SMOKE_PARTS_S     30.0   /* particles/s at drive = 1 */
#define MF_SMOKE_SLOSH_G     1.5    /* g  lateral/braking slosh puff trigger */

static inline double mf_smoke_drive(double base, double load, double rpm_frac,
                                    double puff)
{
    double f;
    if (!isfinite(base) || base <= 0.0) return 0.0;
    load = mf_clamp(isfinite(load) ? load : 0.0, 0.0, 1.0);
    rpm_frac = mf_clamp(isfinite(rpm_frac) ? rpm_frac : 0.0, 0.0, 1.0);
    puff = mf_clamp(isfinite(puff) ? puff : 0.0, 0.0, 1.0);
    f = MF_SMOKE_LOAD_MIN + (1.0 - MF_SMOKE_LOAD_MIN)
        * (MF_SMOKE_LOAD_W * load + MF_SMOKE_RPM_W * rpm_frac);
    return base * (f + MF_SMOKE_PUFF_GAIN * puff);
}

/* One step of the puff envelope; returns the new 0..1 puff and updates the
 * caller's previous-load latch. dt is clamped so a pause/tab-out cannot
 * inflate or erase the envelope. */
static inline double mf_smoke_puff_step(double puff, double load, double dt,
                                        double slosh_g, double *prev_load)
{
    double prev = prev_load ? *prev_load : 0.0;
    if (!isfinite(load)) load = prev;
    load = mf_clamp(load, 0.0, 1.0);
    if (!isfinite(dt) || dt < 0.0) dt = 0.0;
    if (dt > 0.25) dt = 0.25;
    if (!isfinite(puff)) puff = 0.0;
    if ((prev > 0.45 && load < 0.15) || (prev < 0.15 && load > 0.45))
        puff = 1.0;                              /* lift-off or reapply */
    else if (isfinite(slosh_g) && fabs(slosh_g) > MF_SMOKE_SLOSH_G
             && puff < 0.7)
        puff = 0.7;                              /* braking/cornering slosh */
    else {
        puff *= exp(-dt / MF_SMOKE_PUFF_TAU_S);
        if (puff < 0.01) puff = 0.0;
    }
    if (prev_load) *prev_load = load;
    return puff;
}

/* Particle accumulator: returns the whole particles due, keeps the fraction. */
static inline double mf_smoke_acc(double acc, double drive, double dt)
{
    if (!isfinite(acc) || acc < 0.0) acc = 0.0;
    if (!isfinite(drive) || drive <= 0.0 || !isfinite(dt) || dt <= 0.0)
        return acc;
    if (dt > 0.5) dt = 0.5;                  /* pause/stall guard */
    acc += drive * MF_SMOKE_PARTS_S * dt;
    if (acc > MF_SMOKE_PARTS_S) acc = MF_SMOKE_PARTS_S;
    return acc;
}

/* ---- R3o (ADDENDUM 11, Claude review): per-lap fuel estimate --------------
 * The old advice divided the current fuel by a per-tick burn EMA (tau ~1.6 s)
 * times the lap time, so laps_left swung with the last couple of seconds of
 * throttle (straight vs braking). The engineer now measures FUEL USED PER LAP
 * (lap start - lap end; a refuel makes the use negative and the lap is
 * skipped) and divides by the mean of the last 1-2 completed laps. Advice
 * only exists after one full valid lap. */
typedef struct {
    int    lap;          /* last seen lap counter, -1 = not started */
    double fuel0;        /* fuel at the current lap start */
    double t0;           /* sim time at the current lap start */
    double used[2];      /* completed lap uses, [0] newest */
    int    have;         /* valid completed laps, saturates at 2 */
    int    skip;         /* skip the next completed lap (a partial lap after
                          * an arming, no full-lap measurement) */
} MfFuelLap;

static inline void mf_fuel_lap_init(MfFuelLap *s)
{
    s->lap = -1;
    s->fuel0 = 0.0;
    s->t0 = 0.0;
    s->used[0] = s->used[1] = 0.0;
    s->have = 0;
    s->skip = 0;
}

static inline void mf_fuel_lap_step(MfFuelLap *s, int lap, double fuel_l,
                                    double t_s)
{
    if (!isfinite(fuel_l) || !isfinite(t_s)) return;
    if (s->lap < 0) {                        /* first sample */
        s->lap = lap;
        s->fuel0 = fuel_l;
        s->t0 = t_s;
        return;
    }
    if (lap != s->lap) {
        if (s->skip) {
            s->skip = 0;                     /* partial lap (armed mid-lap) */
        } else {
            double used = s->fuel0 - fuel_l;
            double lt = t_s - s->t0;
            if (used > 0.0 && lt > 30.0 && lt < 400.0) {  /* refuel/teleport skip */
                s->used[1] = s->used[0];
                s->used[0] = used;
                if (s->have < 2) s->have++;
            }
        }
        s->lap = lap;
        s->fuel0 = fuel_l;
        s->t0 = t_s;
    }
}

static inline double mf_fuel_lap_avg(const MfFuelLap *s)
{
    if (s->have <= 0) return -1.0;
    if (s->have == 1) return s->used[0];
    return 0.5 * (s->used[0] + s->used[1]);
}

static inline double mf_fuel_laps_left(const MfFuelLap *s, double fuel_l)
{
    double avg = mf_fuel_lap_avg(s);
    if (!(avg > 1e-6) || !isfinite(fuel_l)) return -1.0;
    return fuel_l / avg;
}

/* Advice with hysteresis (0 none, 1 save, 2 pit); `state` is the previous
 * result (to stop SAVE/PIT flipping on estimate wobble). */
static inline int mf_fuel_advice(double laps_left, double laps_remaining,
                                 int state)
{
    if (!isfinite(laps_left) || laps_left < 0.0) return 0;
    if (laps_left < 1.8) return 2;
    if (laps_remaining > 0.0 && laps_left < laps_remaining) {
        if (laps_left >= laps_remaining * 0.85) return 1;
        return 2;
    }
    if (state == 2 && laps_remaining > 0.0 && laps_left < laps_remaining * 1.10)
        return 2;                            /* keep the pit call until a margin */
    if (state == 1 && laps_remaining > 0.0 && laps_left < laps_remaining * 1.05)
        return 1;                            /* keep SAVE until clearly clear */
    return 0;
}

static inline double mf_pow2(double x)
{
    /* guard against overflow before it reaches +inf */
    if (x > 60.0) return 1.15e18;
    if (x < -60.0) return 0.0;
    return pow(2.0, x);
}

/* N5 2026-10-06 (owner-approved): REPAIR clamps a temperature to the car's
 * warm normal and NEVER raises it. Kelvin in/out. */
static inline double mf_repair_clamp_temp(double cur_k, double normal_k)
{
    return cur_k < normal_k ? cur_k : normal_k;
}

/* N7 2026-10-06 (owner-approved): the rpm half of the game's life drain that
 * lies ABOVE the CURRENT limit - the part the over-rev-owner detour credits
 * back so the vanilla over-rev wear becomes the wear AT the limit. Inputs in
 * the game's own units (rad/s, the P+7612 base, the P+7620 inverse range,
 * seconds). 0 when not over the limit or on any non-finite/negative input;
 * clamped to <= 1e6 per call. */
static inline double mf_overrev_excess(double w, double lim, double base,
                                       double inv_range, double dt)
{
    double aw, e;
    if (!(w == w) || !(lim == lim) || !(base == base)
        || !(inv_range == inv_range) || !(dt == dt) || dt <= 0.0
        || !(lim > 0.0))
        return 0.0;
    aw = fabs(w);
    if (!(aw > lim)) return 0.0;
    e = 0.5 * (mf_pow2((aw - base) * inv_range)
               - mf_pow2((lim - base) * inv_range)) * dt;
    if (!(e > 0.0) || !(e < 1e300)) return 0.0;
    if (e > 1e6) e = 1e6;
    return e;
}

static inline uint32_t mf_rng_u32(MfRng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

/* E9-H4a: seed mixer for per-slot draws (splitmix32). The live finding
 * ai-quality-q-linear-in-slot: seeding each AI engine with
 * (tick x constant) ^ slot made adjacent slots' first xorshift32 draw nearly
 * linear (1.043, 1.040, 1.037 ...). Mixing the seed first decorrelates the
 * slots; the same mixer is used by the DLL and by the harness test. */
static inline uint32_t mf_splitmix32(uint32_t x)
{
    x += 0x9E3779B9u;
    x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
    x = (x ^ (x >> 13)) * 0xC2B2AE35u;
    return x ^ (x >> 16);
}

/* uniform [0,1) */
static inline double mf_rng_d(MfRng *r)
{
    return (double)(mf_rng_u32(r) >> 8) / 16777216.0;
}

static inline double mf_rand_range(MfRng *r, double lo, double hi)
{
    return lo + (hi - lo) * mf_rng_d(r);
}

/* Vogel viscosity (material law, absolute constants), normalised so
 * mu(oil_opt) = 1. Hot oil thins. */
static inline double mf_mu(double temp_k, const MfCar *c)
{
    if (temp_k < MF_MU_C_K + 20.0) temp_k = MF_MU_C_K + 20.0;
    return exp(MF_MU_B_K * (1.0 / (temp_k - MF_MU_C_K)
                            - 1.0 / (c->oil_opt_k - MF_MU_C_K)));
}

/* Water vapour pressure, Antoine (material law, absolute), in bar absolute. */
static inline double mf_water_psys_bar(double temp_k)
{
    double tc = temp_k - 273.15;
    double mmhg;
    if (tc <= 100.0)
        mmhg = pow(10.0, 8.07131 - 1730.63 / (233.426 + tc));
    else
        mmhg = pow(10.0, 8.14019 - 1810.94 / (244.485 + tc));
    return mmhg * 0.00133322;
}

/* Boiling point at a system pressure (inverse Antoine, material). */
static inline double mf_water_boil_k(double p_bar)
{
    double mmhg = p_bar / 0.00133322;
    double lg = log10(mmhg > 1.0 ? mmhg : 1.0);
    double tc = 1810.94 / (8.14019 - lg) - 244.485;
    return tc + 273.15;
}

/* ------------------------------------------------------------------ */
/* Car derivation (1.4): relative fallbacks, never throws.            */
/* Returns a bit per substituted field: 1 rev_limit, 2 rpm_base,      */
/* 4 rpm_range, 8 oil_base, 16 oil_range, 32 oil_opt, 64 tq_zero,     */
/* 128 peak_tq, 256 inertia, 512 idle_lo, 1024 idle_hi, 2048          */
/* fuel_cons, 4096 fuel_mult, 8192 owt_k0, 16384 owt_k1.              */
/* tq_zero / peak_tq arrive from the car's RPMTorque table (caller:   */
/* 0 when the table is missing or unreadable).                        */
/* E9e: inertia, idle band, FuelConsumption and OilWaterHeatTransfer  */
/* arrive from the player engine object (section 2 of the addendum).  */
/* fuel_cons = 0 and owt_k0 = 0 are the "missing" fallbacks.          */
/* ------------------------------------------------------------------ */

static inline unsigned mf_car_derive(MfCar *c, double rev_limit, double rpm_base,
                                     double rpm_range, double oil_base,
                                     double oil_range, double oil_opt,
                                     double tq_zero, double peak_tq,
                                     double inertia, double idle_lo,
                                     double idle_hi, double fuel_cons,
                                     double fuel_mult, double owt_k0,
                                     double owt_k1)
{
    unsigned f = 0;
    memset(c, 0, sizeof *c);
    c->rev_limit_rads = rev_limit;
    if (!(c->rev_limit_rads > 100.0 && c->rev_limit_rads < 10000.0)) {
        c->rev_limit_rads = 1800.0; f |= 1u;      /* relative fallback: typical F1 limit */
    }
    c->rpm_base_rads = rpm_base;
    if (!(c->rpm_base_rads > 100.0 && c->rpm_base_rads < c->rev_limit_rads * 2.0)) {
        c->rpm_base_rads = c->rev_limit_rads * 0.9; f |= 2u;
    }
    c->rpm_range_rads = rpm_range;
    if (!(c->rpm_range_rads > 1.0 && c->rpm_range_rads < 500.0)) {
        c->rpm_range_rads = 40.0; f |= 4u;        /* relative fallback: typical range */
    }
    c->oil_base_k = oil_base;
    if (!(c->oil_base_k > 330.0 && c->oil_base_k < 500.0)) {
        c->oil_base_k = 396.0; f |= 8u;           /* relative fallback: typical base */
    }
    c->oil_range_k = oil_range;
    if (!(c->oil_range_k > 0.5 && c->oil_range_k < 30.0)) {
        c->oil_range_k = 2.9; f |= 16u;
    }
    c->oil_opt_k = oil_opt;
    if (!(c->oil_opt_k > 330.0 && c->oil_opt_k < c->oil_base_k)) {
        c->oil_opt_k = c->oil_base_k - 15.0; f |= 32u;
    }
    c->w_oil_ref = mf_pow2((c->oil_opt_k - c->oil_base_k) / c->oil_range_k);
    /* E9a: torque curve shape from the car's RPMTorque table. The torque-zero
     * rpm must sit above the limiter (only an over-rev can reach it, never
     * normal driving); the peak between 0.3 x limit and the zero crossing. */
    c->tq_zero_rads = tq_zero;
    if (!(c->tq_zero_rads > c->rev_limit_rads * 1.02
          && c->tq_zero_rads < c->rev_limit_rads * 2.5)) {
        c->tq_zero_rads = c->rev_limit_rads * MF_TQ_ZERO_FALLBACK; f |= 64u;
    }
    c->peak_tq_rads = peak_tq;
    if (!(c->peak_tq_rads > c->rev_limit_rads * 0.3
          && c->peak_tq_rads < c->tq_zero_rads)) {
        c->peak_tq_rads = c->rev_limit_rads * MF_TQ_PEAK_FALLBACK; f |= 128u;
    }
    /* E9e: EngineInertia (kg m2, 0.005-1.0), fallback 0.05 */
    c->inertia_kgm2 = inertia;
    if (!(c->inertia_kgm2 > 0.005 && c->inertia_kgm2 < 1.0)) {
        c->inertia_kgm2 = MF_INERTIA_FALLBACK; f |= 256u;
    }
    /* E9e: IdleRPMLogic band (rad/s, 50 .. 0.5 x limit, LO < HI),
     * fallback 0.2 / 0.25 x limit */
    c->idle_lo_rads = idle_lo;
    if (!(c->idle_lo_rads >= 50.0 && c->idle_lo_rads <= c->rev_limit_rads * 0.5)) {
        c->idle_lo_rads = c->rev_limit_rads * 0.2; f |= 512u;
    }
    c->idle_hi_rads = idle_hi;
    if (!(c->idle_hi_rads >= 50.0 && c->idle_hi_rads <= c->rev_limit_rads * 0.5)
        || c->idle_hi_rads <= c->idle_lo_rads) {
        c->idle_hi_rads = c->rev_limit_rads * 0.25; f |= 1024u;
    }
    if (c->idle_hi_rads <= c->idle_lo_rads)    /* a real LO above the fallback HI */
        c->idle_hi_rads = c->idle_lo_rads * 1.2;
    /* E9e: FuelConsumption (L per rad/s per s, 1e-7..1e-3) + multiplier
     * (0..10). Missing cons keeps the session-peak flow reference (fallback). */
    c->fuel_cons = fuel_cons;
    if (!(c->fuel_cons > 1e-7 && c->fuel_cons < 1e-3)) {
        c->fuel_cons = 0.0; f |= 2048u;
    }
    c->fuel_mult = fuel_mult;
    if (!(c->fuel_mult >= 0.0 && c->fuel_mult <= 10.0)) {
        c->fuel_mult = 1.0; f |= 4096u;
    }
    /* E9e: OilWaterHeatTransfer (base 1e-5..1, per rad/s 0..1e-2).
     * Missing (fallback) = no gasket-coupling effect. */
    c->owt_k0 = owt_k0;
    if (!(c->owt_k0 >= 1e-5 && c->owt_k0 <= 1.0)) {
        c->owt_k0 = 0.0; f |= 8192u;
    }
    c->owt_k1 = owt_k1;
    if (!(c->owt_k1 >= 0.0 && c->owt_k1 <= 1e-2)) {
        c->owt_k1 = 0.0; f |= 16384u;
    }
    c->def_flags = f;
    return f;
}

/* P0 (ADDENDUM 2, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt; plan
 * docs/FAILURE_OCCURRENCE_HAZARD_PLAN_2026-10-01.md): standard normal CDF,
 * used by the sweep's vanilla-AI failure probability
 * Phi((T x f - 3 x Avg) / (3 x Var)). Pure, no behaviour change. */
static inline double mf_phi(double x)
{
    return 0.5 * erfc(-x / 1.41421356237309504880);
}

/* J3 (docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): parse an engine.ini "(base,
 * range)" text value such as "(15900.0, 360.0)" or "( 120.0 , 3.0 )". Both
 * numbers must be finite; only spaces, brackets and one comma may surround
 * them (trailing junk is rejected). Pure - the DLL's H5 parse and the harness
 * unit test call the same function. Returns 1 on success. */
static inline int mf_pair_parse(const char *s, double *out_a, double *out_b)
{
    char *end;
    double a, b;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t' || *s == '(') s++;
    a = strtod(s, &end);
    if (end == s) return 0;
    s = end;
    while (*s == ' ' || *s == '\t') s++;
    if (*s != ',') return 0;
    s++;
    while (*s == ' ' || *s == '\t') s++;
    b = strtod(s, &end);
    if (end == s) return 0;
    s = end;
    while (*s == ' ' || *s == '\t' || *s == ')') s++;
    if (*s != '\0' && *s != '\r' && *s != '\n') return 0;
    if (!isfinite(a) || !isfinite(b)) return 0;
    *out_a = a;
    *out_b = b;
    return 1;
}

/* E9e (addendum 2 section 3 item 7): the torque-curve power multiplier the
 * fuelcut hook applies per physics step. s is the smoothstep from the car's
 * peak-torque rpm to its rev limit; the losses are set by the active fault
 * (valve return / fuel pressure: top; bearings: all; rings: load). Never
 * raises torque; floor MF_POWER_MIN. */
static inline double mf_power_shape_mult(const MfCar *c, const MfOut *o,
                                         double rpm, double throttle)
{
    double s;
    double m;

    if (!(rpm > c->peak_tq_rads)) {
        s = 0.0;
    } else if (!(c->rev_limit_rads > c->peak_tq_rads) || rpm >= c->rev_limit_rads) {
        s = 1.0;
    } else {
        double x = (rpm - c->peak_tq_rads) / (c->rev_limit_rads - c->peak_tq_rads);
        s = x * x * (3.0 - 2.0 * x);              /* smoothstep */
    }
    m = 1.0 - o->loss_all - o->loss_top * s
        - o->loss_load * mf_clamp(throttle, 0.0, 1.0);
    if (m > 1.0) m = 1.0;
    if (m < MF_POWER_MIN) m = MF_POWER_MIN;
    return m;
}

/* E9e item 3 / E9d: light engines dip harder on a misfire: the base depth
 * scales by clamp(0.05 / inertia, 0.5, 2.0), capped at MF_MISFIRE_MAX. */
static inline double mf_misfire_depth_scaled(const MfCar *c, double base)
{
    double inertia = c->inertia_kgm2 > 1e-6 ? c->inertia_kgm2 : MF_INERTIA_FALLBACK;
    double s = MF_INERTIA_REF / inertia;
    s = mf_clamp(s, MF_MISFIRE_DIP_MIN, MF_MISFIRE_DIP_MAX);
    return mf_clamp(mf_clamp(base, 0.0, MF_MISFIRE_MAX) * s, 0.0, MF_MISFIRE_MAX);
}

/* E9b-2 item 9: discrete misfire cuts - during a cut the engine output is
 * multiplied by (1 - strength), strength = min(1, MF_MISFIRE_CUT_GAIN x depth).
 * At depth 0.6: strength 0.9 with 2 events/s of 40-120 ms mean 80 ms:
 * average loss 2 x 0.08 x 0.9 = 14.4 %, under the 15 % bound (asserted in
 * engine_model_test.c with this same formula). */
static inline double mf_misfire_cut_strength(double depth)
{
    double s = MF_MISFIRE_CUT_GAIN * (depth > 0.0 ? depth : 0.0);
    return s > 1.0 ? 1.0 : s;
}

/* E9d item 2: worn bearings raise the game's own engine-braking map, so the
 * car decelerates harder off throttle. The DLL writes ENG_BRAKE_MAP =
 * saved x this factor and restores it by ownership. */
static inline double mf_friction_factor(double h_b)
{
    double f = 1.0 + MF_FRICTION_K * (1.0 - mf_clamp(h_b, 0.0, 1.0));
    return f < 1.0 ? 1.0 : f;
}

/* The game's own drain law (handoff 1.1). s per s. Used by E1's simulated
 * clock and as the relative fallback when the measured drain is missing. */
static inline double mf_game_drain(const MfCar *c, double rpm, double oil_k, double stress)
{
    double w_rpm = mf_pow2((rpm - c->rpm_base_rads) / c->rpm_range_rads);
    double w_oil = mf_pow2((oil_k - c->oil_base_k) / c->oil_range_k);
    return 0.5 * (w_rpm + w_oil) + (stress > 0.0 ? stress : 0.0);
}

/* ------------------------------------------------------------------ */
/* E9p: AI native-law coefficients and step (addendum 2 section 4,     */
/* TEST mode - it changes AI reliability).                             */
/* ------------------------------------------------------------------ */

typedef struct {
    double comb_heat;      /* CombustionHeat per unit fuel term (game this+7516) */
    double speed_heat;     /* EngineSpeedHeat per rad/s (this+7524) */
    double min_cool;       /* OilMinimumCooling per K (this+7532) */
    double owt_k0;         /* OilWaterHeatTransfer base (this+7548) */
    double owt_k1;         /* OilWaterHeatTransfer per rad/s (this+7556) */
    double rad_k0;         /* WaterMinimumCooling / radiator base (this+7572) */
    double rad_k1;         /* RadiatorCooling airflow term (this[774]) */
    double stat_target_k;  /* thermostat target = optimum oil - 10 K */
    double amb_k;          /* ambient air, K */
} MfAiLawCoef;

typedef struct {
    double life_s;         /* shadow life on the game's own drain law */
    double oil_k;
    double water_k;
    int    init;
} MfAiLaw;

/* One native-law step. J1 (docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): law mode is
 * THERMAL ONLY - the game AI timer (veh+0x3D70) is the life clock in both
 * modes, so no life drain happens here; MfAiLaw.life_s is kept for the panel
 * and set by the caller. This is only the thermal ODE structure of
 * EngineStressSimulation 0x43EF20 (combustion x fuel term, speed heat,
 * minimum cooling, oil->water transfer, radiator K0 + K1 x airflow) with the
 * observed thermostat pull toward optimum - 10 K. The exact per-call unit
 * chain of the game function is not fully established (see the E9p log
 * entry); this reproduces the live settling behaviour in TEST mode only. */
static inline void mf_ai_law_step(MfAiLaw *s, const MfAiLawCoef *k, const MfCar *c,
                                  double rpm, double throttle, double speed,
                                  double dt)
{
    double fuel_term, heat_in, owt, transfer, cool;

    (void)c;   /* J1: the law no longer reads the car (life clock removed) */
    if (!s->init) {
        s->oil_k = k->stat_target_k;
        s->water_k = k->stat_target_k - 15.0;
        s->init = 1;
    }
    fuel_term = throttle * MF_AI_LAW_FUEL_LPS;
    heat_in = fuel_term * k->comb_heat
            + fabs(rpm) * k->speed_heat
            + (k->amb_k - s->oil_k) * k->min_cool;
    owt = k->owt_k0;
    if (s->oil_k > k->stat_target_k + 10.0)
        owt += fabs(rpm) * k->owt_k1;
    transfer = owt * (s->oil_k - s->water_k);
    s->oil_k += (heat_in - transfer) * dt;
    cool = (speed * k->rad_k1 + k->rad_k0) * (k->amb_k - s->water_k);
    s->water_k += (transfer + cool) * dt;
    s->oil_k += (k->stat_target_k - s->oil_k) * MF_AI_LAW_THERM_K * dt;
}

/* J1e (docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): a finished AI slot (retired or
 * terminal written) is never stepped again - law or model. ai_step_slot's
 * guard uses this; the harness asserts the rule directly. */
static inline int mf_ai_slot_active(int slot, int done)
{
    return slot >= 1 && !done;
}

/* Q1 (ADDENDUM 3, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): AI wear rate -
 * the car's own game-law drain divided by the FIELD mean (EMA tau 600 s of
 * the mean drain over all running AI slots, valid after 60 s). Clamped
 * [0.25, 4.0]; 1.0 until the field mean exists, so the field average car
 * drains exactly 1 s/s and the vanilla failure count is kept. */
#define MF_AI_RATE_MIN         0.25
#define MF_AI_RATE_MAX         4.0
static inline double mf_ai_wear_rate(double drain_law, double field_mean,
                                     int field_valid)
{
    double r;
    if (!field_valid || !(field_mean > 0.0)) return 1.0;
    r = drain_law / field_mean;
    if (!isfinite(r)) return 1.0;
    return mf_clamp(r, MF_AI_RATE_MIN, MF_AI_RATE_MAX);
}

/* Q2 (ADDENDUM 3, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): game timer
 * ownership. While an AI slot is live the DLL keeps veh+0x3D70 (the game's
 * AI engine timer) >= ai_life + 600 s so the vanilla roll cannot kill a car
 * our clock keeps alive; when below, the timer is written to ai_life + 1200.
 * The write only happens for a live slot of the CURRENT generation (a done
 * slot or a stale generation is never written). The terminal path still
 * writes 0.001. */
static inline int mf_ai_timer_needs_own(float timer, double ai_life)
{
    return (double)timer < ai_life + 600.0;
}

static inline float mf_ai_timer_own_value(double ai_life)
{
    return (float)(ai_life + 1200.0);
}

static inline int mf_ai_timer_own_ok(int done, unsigned slot_gen,
                                     unsigned cur_gen)
{
    return !done && slot_gen == cur_gen;
}

/* 2026-10-07 audit MF-02 (owner D2): value to put back into the game's
 * AI timer when MF stops owning it. Returns 1 and sets *out only when the
 * timer still looks like OUR padding: we wrote it (written > 0), the game
 * has only drained it since (cur <= written + 0.5) and it is above MF's
 * own remaining life in game units (ai_life x div). The game then keeps
 * draining from MF's clock instead of from the padded value. */
static inline int mf_ai_timer_release_value(double cur, double written,
                                            double ai_life, double div,
                                            double *out)
{
    double vanilla;
    if (!(written > 0.0) || !isfinite(cur) || !isfinite(ai_life)) return 0;
    if (!(div > 0.0)) div = 1.0;
    vanilla = ai_life * div;
    if (!(vanilla >= 0.0)) return 0;
    if (cur > written + 0.5) return 0;   /* the game re-armed it */
    if (!(cur > vanilla)) return 0;      /* nothing to take off */
    *out = vanilla;
    return 1;
}

/* Q7c (ADDENDUM 7, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): scripted vanilla
 * drop. The game's RaceEvents season script (docs/VANILLA_RACE_EVENTS_FINDING_
 * 2026-10-01.md) writes a small finite life where a large/infinite one was
 * (EngineWearMode 3 seeds FLT_MAX; AIMainUpdate writes rand % max(bestLap,40)).
 * prev = the previous pass's raw timer, now = this pass's, own_written = the
 * last value WE wrote (mf_ai_timer_own_value; 0 when we never wrote). A > 50 %
 * fall that is not our own write is treated as a scripted event. Mode 3: any
 * finite value after FLT_MAX is such a drop. Now <= 0 is the game's own kill,
 * never a scripted write. */
static inline int mf_ai_scripted_drop(double prev, double now, double own_written)
{
    if (!(now > 0.0) || !(now < 1e29)) return 0;      /* finite, positive only */
    if (prev < 1e29 && !(now < 0.5 * prev)) return 0; /* not a > 50 % fall */
    if (fabs(now - own_written) <= MF_DT_MAX_S + 1.0) return 0; /* our write */
    return 1;
}

/* R1a (ADDENDUM 9, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): the oil-heat lever
 * want. The pump may only act on an output from a model step that actually
 * ran; on a non-step tick (pause, a stalled pump, the car-valid gate) the
 * latch keeps the last want and never releases the lever, so the stock is
 * saved once and restored once per leak episode. A stepped output <= 0
 * clears the want (no heat requested) without releasing the lever. */
static inline double mf_oil_heat_latch(double latched_ks, double out_ks,
                                       int stepped)
{
    if (!stepped) return latched_ks;
    return (out_ks > 0.0 && isfinite(out_ks)) ? out_ks : 0.0;
}

/* R1b (ADDENDUM 9): a worker pass where the game sim clock did not advance
 * (pause, menus) - every lever pump returns before its game writes and logs,
 * and every hold state stays frozen. */
static inline int mf_sim_advanced(unsigned prev_ms, unsigned now_ms)
{
    return now_ms != prev_ms;
}

/* ------------------------------------------------------------------ */
/* Session / fault rolls (1.2, 1.3)                                    */
/* ------------------------------------------------------------------ */

static inline void mf_engine_init(MfEngine *e, uint32_t seed)
{
    memset(e, 0, sizeof *e);
    e->rng.s = seed ? seed : 0x9E3779B9u;
    e->lead_s = mf_rand_range(&e->rng, MF_LEAD_MIN_S, MF_LEAD_MIN_S + MF_LEAD_SPAN_S);
    /* E9b-2 item 8: engine quality, triangular LO-HI with mode 1.0. Drawn from
     * a COPY of the state so the main draw sequence (lead, fault, severity,
     * ...) stays exactly as the earlier batches calibrated it. */
    {
        MfRng qr = e->rng;
        double u = mf_rng_d(&qr);
        double mid = (MF_QUALITY_MODE - MF_QUALITY_LO)
                     / (MF_QUALITY_HI - MF_QUALITY_LO);
        if (u < mid)
            e->quality = MF_QUALITY_LO
                         + sqrt(u * (MF_QUALITY_HI - MF_QUALITY_LO)
                                * (MF_QUALITY_MODE - MF_QUALITY_LO));
        else
            e->quality = MF_QUALITY_HI
                         - sqrt((1.0 - u) * (MF_QUALITY_HI - MF_QUALITY_LO)
                                * (MF_QUALITY_HI - MF_QUALITY_MODE));
    }
    /* P2: separate shadow-hazard stream (main draw sequence untouched) */
    e->hz_rng.s = mf_splitmix32((e->rng.s ^ 0xA5A5A5A5u) | 1u);
    (void)mf_rng_u32(&e->hz_rng);  /* decorrelate from the main stream */
    e->budget_s = 0.0;
    e->hz_x = 0.0;
    e->hz_beta = MF_HAZARD_BETA;
    e->hz_eta = MF_HAZARD_ETA;
    e->shadow_fire_t = -1.0;
    e->shadow_fired = 0;
    e->shadow_fault = 0;
    e->grade = 1.0;                /* E9q: field median until the caller grades */
    e->severity = MF_SEV_MEDIUM;
    e->fault = MF_FAULT_NONE;
    e->valve_m = 1.0;
    e->valve_a = MF_VALVE_LEAK_A0;
    e->h_vt = 1.0;
    e->pump_eta = 1.0;
    e->v_oil_l = MF_OIL_START_L;
    e->oil_a = 1.0;
    e->h_b = 1.0;
    e->film_last = 1.0;
    e->f_rad = 1.0;
    e->v_w_l = MF_WATER_START_L;
    e->water_a = 1.0;
    e->h_g = 1.0;
    e->ring_s = 1.0;
    e->crack_a = 0.0;
    e->fuel_cap = 1.0;
    e->h_rod = 1.0;
    e->life_prev_s = 0.0;
    e->overrev_last_t_s = -1e9;    /* E9a: no over-rev jump yet */
    e->t_exh_k = MF_AMBIENT_FALLBACK_K;
    e->t_exh_peak_k = MF_AMBIENT_FALLBACK_K;
    e->t_exh_trough_k = MF_AMBIENT_FALLBACK_K;
    e->t_pis_k = MF_AMBIENT_FALLBACK_K;
    e->t_ev_k = MF_AMBIENT_FALLBACK_K;
    e->t_brg_k = MF_AMBIENT_FALLBACK_K;
    e->t_head_k = MF_AMBIENT_FALLBACK_K;
    e->shift_n = 0;
}

/* Severity and stabilise roll (1.3.4, 1.3.7); exposed for distribution tests. */
static inline int mf_roll_severity(MfEngine *e, int *stabilise)
{
    double r = mf_rng_d(&e->rng);
    int sev = (r < MF_SEV_P_SMALL) ? MF_SEV_SMALL
            : (r < MF_SEV_P_SMALL + MF_SEV_P_MEDIUM) ? MF_SEV_MEDIUM : MF_SEV_BIG;
    if (stabilise) *stabilise = mf_rng_d(&e->rng) < MF_STABILISE_P;
    return sev;
}

static inline double mf_sev_gain(int sev)
{
    if (sev == MF_SEV_SMALL) return MF_SEV_GAIN_0;
    if (sev == MF_SEV_BIG)   return MF_SEV_GAIN_2;
    return MF_SEV_GAIN_1;
}

/* P1 (ADDENDUM 2): the fault weights, ONE expression used by BOTH the onset
 * draw (mf_roll_fault) and the damage accumulators (mf_dmg_accum) - the
 * expressions below are byte-for-byte the pre-P1 mf_roll_fault ones. Returns
 * the sum of w[1..]. */
static inline double mf_fault_weights(const MfEngine *e, double w[MF_FAULT_COUNT])
{
    static const double base[MF_FAULT_COUNT] = {
        0.0,   /* none */
        15.0,  /* valve return: revs */
        15.0,  /* oil pressure: oil heat + g */
        15.0,  /* oil leak: oil heat */
        10.0,  /* water leak: water heat */
        5.0,   /* radiator debris: g (x3 after off-track) */
        10.0,  /* fuel pressure */
        5.0,   /* fuel leak: thermal cycling */
        15.0,  /* blow-by: revs + oil heat */
        10.0,  /* exhaust crack: thermal cycling */
        0.0    /* blow-up (N3): AI-only, added by mf_pick_onset_fault */
    };
    double rev = e->share_rev, oil = e->share_oil, wat = e->share_water;
    double cyc = e->share_thermal, gl = e->share_g;
    double sum = rev + oil + wat + cyc + gl;
    double total = 0.0;
    int i;
    if (sum <= 0.0) sum = 1.0;                     /* no history: flat shares */
    rev /= sum; oil /= sum; wat /= sum; cyc /= sum; gl /= sum;
    w[0] = 0.0;
    w[MF_FAULT_VALVE_AIR]     = base[MF_FAULT_VALVE_AIR]     * (1.0 + 2.0 * rev);
    w[MF_FAULT_OIL_PRESSURE]  = base[MF_FAULT_OIL_PRESSURE]  * (1.0 + 2.0 * oil) * (1.0 + 2.0 * gl);
    w[MF_FAULT_OIL_LEAK]      = base[MF_FAULT_OIL_LEAK]      * (1.0 + 2.0 * oil);
    w[MF_FAULT_WATER_LEAK]    = base[MF_FAULT_WATER_LEAK]    * (1.0 + 2.0 * wat);
    w[MF_FAULT_RAD_DEBRIS]    = base[MF_FAULT_RAD_DEBRIS]    * (1.0 + 2.0 * gl);
    w[MF_FAULT_FUEL_PRESSURE] = base[MF_FAULT_FUEL_PRESSURE];
    w[MF_FAULT_FUEL_LEAK]     = base[MF_FAULT_FUEL_LEAK]     * (1.0 + 2.0 * cyc);
    w[MF_FAULT_BLOWBY]        = base[MF_FAULT_BLOWBY]        * (1.0 + 2.0 * rev) * (1.0 + 2.0 * oil);
    w[MF_FAULT_EXHAUST]       = base[MF_FAULT_EXHAUST]       * (1.0 + 2.0 * cyc);
    w[MF_FAULT_BLOWUP]        = base[MF_FAULT_BLOWUP];   /* N3: 0 - AI-only */
    if (e->offtrack_timer_s > 0.0)
        w[MF_FAULT_RAD_DEBRIS] *= 3.0;             /* off-track excursion boost (1.2) */
    for (i = 1; i < MF_FAULT_COUNT; i++) total += w[i];
    return total;
}

/* P1: book the life seconds consumed this step into the fault accumulators by
 * the same weights. sum(dmg) == dmg_total by construction. */
static inline void mf_dmg_accum(MfEngine *e, double consumed)
{
    double w[MF_FAULT_COUNT];
    double total;
    int i;
    if (!(consumed > 0.0) || !isfinite(consumed)) return;
    total = mf_fault_weights(e, w);
    if (!(total > 0.0)) return;
    for (i = 1; i < MF_FAULT_COUNT; i++)
        e->dmg[i] += consumed * w[i] / total;
    e->dmg_total += consumed;
}

/* P2 (ADDENDUM 2): one hazard step. P(fire in [x_prev, x_now]) =
 * 1 - exp(-( (x_now/eta)^beta - (x_prev/eta)^beta )); u uniform. x is
 * monotonic (dmg_total/budget). Shadow only - P3 is an owner decision. */
static inline int mf_hazard_step(double x_prev, double x_now, double beta,
                                 double eta, double u)
{
    double p;
    if (!(eta > 0.0) || !(beta > 0.0) || !isfinite(x_now) || !isfinite(x_prev))
        return 0;
    if (x_prev < 0.0) x_prev = 0.0;
    if (x_now < x_prev) x_now = x_prev;
    p = 1.0 - exp(-(pow(x_now / eta, beta) - pow(x_prev / eta, beta)));
    if (!(p > 0.0)) return 0;
    if (p > 1.0) p = 1.0;
    return u < p;
}

/* Weighted fault roll (1.2). Uses the last-600 s stress shares. The weight
 * expressions live in mf_fault_weights (P1); the rng draw order is unchanged:
 * exactly one mf_rng_d per roll, then the same cumulative walk. */
static inline int mf_roll_fault(MfEngine *e)
{
    double w[MF_FAULT_COUNT];
    double total = mf_fault_weights(e, w);
    double r = mf_rng_d(&e->rng) * total;
    int i;
    for (i = 1; i < MF_FAULT_COUNT; i++) {
        if (r < w[i]) return i;
        r -= w[i];
    }
    return MF_FAULT_BLOWBY;
}

/* Q3 (ADDENDUM 3, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): fault from the
 * accumulated damage history - one draw on the P2 hz_rng stream, walked over
 * the dmg shares (exactly the P2 shadow-fault draw). Falls back to the
 * stressor roll when there is no history yet. */
static inline int mf_fault_history(MfEngine *e)
{
    double r, acc = 0.0;
    int i, f = MF_FAULT_BLOWBY;
    if (!(e->dmg_total > 0.0)) return mf_roll_fault(e);
    r = mf_rng_d(&e->hz_rng) * e->dmg_total;
    for (i = 1; i < MF_FAULT_COUNT; i++) {
        acc += e->dmg[i];
        if (r < acc) { f = i; break; }
    }
    return f;
}

/* N3c (owner D3/D5): the fault drawn when an AI onset fires. BLOW-UP takes
 * MF_AI_BLOWUP_WEIGHT of the AI onset mass; the other faults keep their
 * relative proportions (the 1 - weight that remains), so the sum stays 1.
 * The player path (ai_mode 0) draws exactly as before - no extra rng draw,
 * no BLOW-UP - and "Failure Rate none" never reaches here (life_infinite /
 * the DLL gate stop every onset path upstream). */
static inline int mf_pick_onset_fault(MfEngine *e, const MfInputs *in)
{
    if (in->ai_mode
        && mf_rng_d(&e->rng) < MF_AI_BLOWUP_WEIGHT)
        return MF_FAULT_BLOWUP;
    return in->onset_model ? mf_fault_history(e) : mf_roll_fault(e);
}

/* Applies a rolled/forced onset. stop_delay_s < 0 disables stabilisation. */
static inline void mf_onset_apply(MfEngine *e, const MfInputs *in, int fault,
                                  int severity, double stop_delay_s)
{
    e->onset = 1;
    e->fault = fault;
    /* AUDITFIX: capture unknown thermal normal AT onset, not a later hot sample. */
    if ((fault == MF_FAULT_WATER_LEAK || fault == MF_FAULT_RAD_DEBRIS)
        && (e->water_base_k <= 0.0
            || (e->water_base_k < 273.15 + 85.0 && in->water_k >= 273.15 + 85.0
                && in->water_k >= e->water_base_k + MF_WATER_WARN_DK)))
        e->water_base_k = (in->water_k > 273.15 + 85.0 ? in->water_k : 273.15 + 85.0);
    if ((fault == MF_FAULT_OIL_PRESSURE || fault == MF_FAULT_OIL_LEAK)
        && e->oil_base_k <= 0.0)
        e->oil_base_k = -(in->oil_k > 1.0 ? in->oil_k : 1.0); /* captured; optimum applied at step */
    e->severity = severity;
    e->onset_t_s = e->t_s;
    e->life_onset_s = in->life_s > 1.0 ? in->life_s : 1.0;
    e->drain_onset_sps = e->drain_ema_sps > MF_DRAIN_MIN_SPS ? e->drain_ema_sps : MF_DRAIN_MIN_SPS;
    /* E9b item 1: the fairness floor is measured against the time-to-fail the
     * game's clock predicted at onset (life / drain at that moment) */
    e->ttf_onset_s = e->life_onset_s / e->drain_onset_sps;
    e->arm_virtual = 0;            /* mf_arm_budget sets it after this call */
    e->arm_reanchor = 0;
    e->stabilised = 0;
    e->stop_delay_s = stop_delay_s;
    /* N3b: the BLOW-UP rough-running warning is rolled once at onset
     * (0..MF_AI_BLOWUP_WARN_MAX_S); every other fault keeps 0. The draw is
     * conditional, so the calibrated draw sequence of every other fault is
     * untouched. */
    e->blowup_warn_s = (e->fault == MF_FAULT_BLOWUP)
        ? mf_rng_d(&e->rng) * MF_AI_BLOWUP_WARN_MAX_S : 0.0;
    /* thermal-cycle tracking starts here: no stale warm-up swing (4.5) */
    e->t_exh_peak_k = e->t_exh_k;
    e->t_exh_trough_k = e->t_exh_k;
    e->t_exh_dir = 0.0;
    /* physical size at onset by severity (1.3.4) */
    switch (e->fault) {
    case MF_FAULT_VALVE_AIR:
        e->valve_a = (severity == MF_SEV_SMALL ? 0.0005 :
                      severity == MF_SEV_BIG ? 0.0025 : MF_VALVE_LEAK_A0);
        break;
    case MF_FAULT_RAD_DEBRIS:
        e->f_rad = 1.0 - (severity == MF_SEV_SMALL ? MF_DEBRIS_BLOCK_SMALL :
                          severity == MF_SEV_BIG ? MF_DEBRIS_BLOCK_BIG : MF_DEBRIS_BLOCK_MEDIUM);
        break;
    case MF_FAULT_EXHAUST:
        e->crack_a = (severity == MF_SEV_SMALL ? MF_CRACK_A_SMALL :
                      severity == MF_SEV_BIG ? MF_CRACK_A_BIG : MF_CRACK_A_MEDIUM);
        break;
    default:
        break;
    }
}

/* E9b-2 item 10: per-fault growth scaling for virtual-budget arms. The live
 * calibration (owner 2026-09-30, 416 s at budget 300) applies to the three
 * faults the item measures; every other fault keeps 1.0 (their chains were
 * calibrated by E8g and are not part of the honest-budget item). Natural
 * onsets never use this: only mf_arm_budget (manual / early / impact). */
static inline double mf_budget_gain(int fault)
{
    switch (fault) {
    case MF_FAULT_OIL_PRESSURE:
    case MF_FAULT_VALVE_AIR:
    case MF_FAULT_BLOWBY:
        return MF_BUDGET_GROWTH_K;
    default:
        return 1.0;
    }
}

/* Development of a virtual-budget arm (manual trigger, early failure, impact
 * fault). The budget is the intended time-to-terminal at race pace: the fault
 * develops at `budget_s / gain` life-seconds (clock_drive = gain / budget),
 * while the fairness floor keeps the budget as its ttf. stop_delay_s < 0
 * disables stabilisation. */
static inline void mf_arm_budget(MfEngine *e, const MfInputs *in, int fault,
                                 int severity, double budget_s, double stop_delay_s)
{
    double d = e->drain_ema_sps > MF_ARM_DRAIN_REF ? e->drain_ema_sps : MF_ARM_DRAIN_REF;
    double g = mf_budget_gain(fault);
    if (!(g > 0.0)) g = 1.0;
    mf_onset_apply(e, in, fault, severity, stop_delay_s);
    e->life_onset_s = budget_s * d / g;
    e->drain_onset_sps = d;
    e->ttf_onset_s = budget_s;    /* E9b item 1: the virtual budget IS the ttf */
    e->arm_virtual = 1;
    e->arm_reanchor = e->drain_ema_sps < MF_ARM_DRAIN_REF;   /* cold arm only */
    e->arm_budget_s = budget_s;
    e->arm_gain = g;
}

/* Forced onset for the E4 manual test triggers and E1 target tests.
 * Ignores the game clock by design (test use only); no stabilise roll, so a
 * manual trigger always develops to its terminal state. E4 passes
 * MF_MANUAL_BUDGET_S. */
static inline void mf_fault_arm(MfEngine *e, const MfInputs *in, int fault,
                                int severity, double budget_s)
{
    mf_arm_budget(e, in, fault, severity, budget_s, -1.0);
}

/* E9b-2 item 6: one game-second's early-failure hazard roll (bathtub curve).
 * lambda = MF_EARLY_PER_H / 3600 x clamp(drain, 1, ABUSE_MAX) x (1/q)^2
 * x (1/G)^2 (E9q grade) x MF_SAVE_EARLY while E9i save mode is on. Always
 * draws from the model rng so tests are repeatable. */
static inline int mf_early_roll(MfEngine *e, double drain_sps, int save_mode)
{
    double q = mf_clamp(e->quality, 0.5, 2.0);
    double g = mf_clamp(e->grade > 1e-6 ? e->grade : 1.0,
                        MF_GRADE_MIN, MF_GRADE_MAX);
    double lam = MF_EARLY_PER_H / 3600.0
                 * mf_clamp(drain_sps, 1.0, MF_EARLY_ABUSE_MAX)
                 / (q * q) / (g * g)
                 * (save_mode ? MF_SAVE_EARLY : 1.0);
    return mf_rng_d(&e->rng) < lam;
}

/* ------------------------------------------------------------------ */
/* E9q: engine grade from the car's own reliability data (LifetimeAvg,  */
/* LifetimeVar). The grade is RELATIVE to the session field median and  */
/* affects only HOW (never the game clock). Pure functions, no I/O.     */
/* ------------------------------------------------------------------ */

/* E9q 2.1: score = 0.7 ln(E) + 0.3 ln(Ep),
 * Ep = max(E - LifetimeVar, 0.25 E). Missing/invalid data returns 0 and the
 * field function flags it. */
static inline double mf_grade_score(double life_avg_s, double life_var_s)
{
    double e, ep;
    /* E9-H2c (R13): nonfinite data is invalid, never scored (inf - inf
     * produced a NaN grade in the Codex probe) */
    if (!isfinite(life_avg_s) || !isfinite(life_var_s)) return 0.0;
    if (!(life_avg_s > 1.0) || life_var_s < 0.0) return 0.0;
    e = life_avg_s;
    ep = e - life_var_s;
    if (ep < MF_GRADE_EP_FLOOR * e) ep = MF_GRADE_EP_FLOOR * e;
    return 0.7 * log(e) + 0.3 * log(ep);
}

/* E9q 2.1: G[i] = clamp(exp(score_i - median score), MIN, MAX). Cars with
 * missing/invalid LifetimeAvg/Var keep G = 1.0 (flag) and are EXCLUDED from
 * the median, so one data-less car (e.g. a safety car) cannot void a field.
 * Returns the number of cars that were graded; 0 or 1 valid car = no field
 * grade (flag), every g_out stays 1.0. Median = middle value (odd m) or the
 * mean of the two middle values (even m). */
static inline int mf_grade_field(const double *life_avg_s,
                                 const double *life_var_s, int n, double *g_out)
{
    double sc[MF_GRADE_FIELD_MAX];
    int idx[MF_GRADE_FIELD_MAX];
    double med;
    int i, j, m = 0, nn;

    nn = n > MF_GRADE_FIELD_MAX ? MF_GRADE_FIELD_MAX : n;
    for (i = 0; i < nn; i++) g_out[i] = 1.0;
    if (nn < 1) return 0;
    for (i = 0; i < nn; i++) {
        /* E9-H2c: a nonfinite entry is missing data - keep G = 1.0 and keep it
         * out of the median (mf_grade_score also rejects it defensively) */
        if (isfinite(life_avg_s[i]) && isfinite(life_var_s[i])
            && life_avg_s[i] > 1.0 && life_var_s[i] >= 0.0) {
            sc[m] = mf_grade_score(life_avg_s[i], life_var_s[i]);
            idx[m] = i;
            m++;
        }
    }
    if (m < 2) return 0;                  /* field of one / no data (flag) */
    for (i = 1; i < m; i++) {             /* insertion sort, field is tiny */
        double v = sc[i];
        int k = idx[i];
        for (j = i - 1; j >= 0 && sc[j] > v; j--) {
            sc[j + 1] = sc[j];
            idx[j + 1] = idx[j];
        }
        sc[j + 1] = v;
        idx[j + 1] = k;
    }
    med = (m & 1) ? sc[m / 2] : 0.5 * (sc[m / 2 - 1] + sc[m / 2]);
    for (i = 0; i < m; i++)
        g_out[idx[i]] = mf_clamp(exp(sc[i] - med), MF_GRADE_MIN, MF_GRADE_MAX);
    return m;
}

/* E9q 2.4: optional per-car override (default OFF). The runtime loader owns
 * the file (`plugins/lightloader/mf_grades.ini`, `<name>=<grade>`); the model
 * only applies the value: override_grade > 0 replaces the computed grade,
 * clamped to [MIN, MAX]. */
static inline double mf_grade_override(double computed, double override_grade)
{
    if (override_grade > 0.0)
        return mf_clamp(override_grade, MF_GRADE_MIN, MF_GRADE_MAX);
    return mf_clamp(computed, MF_GRADE_MIN, MF_GRADE_MAX);
}

/* E9b item 3 / E9q: the per-second harmless-niggle hazard, 1/(q x G). */
static inline double mf_niggle_rate(const MfEngine *e)
{
    double q = mf_clamp(e->quality > 1e-6 ? e->quality : 1.0, 0.5, 2.0);
    double g = mf_clamp(e->grade > 1e-6 ? e->grade : 1.0,
                        MF_GRADE_MIN, MF_GRADE_MAX);
    return MF_NIGGLE_PER_H / 3600.0 / (q * g);
}

/* ------------------------------------------------------------------ */
/* Onset step (1.1): drain EMA, lead, the single onset.                */
/* Returns 1 on the step onset fired.                                  */
/* ------------------------------------------------------------------ */

/* 2026-10-06: engine warm enough for an automatic fault onset (see
 * MF_WARM_OIL_DK). Manual test arms ignore it (the DLL logs a cold note). */
static inline int mf_engine_warm(const MfEngine *e, const MfCar *c, const MfInputs *in)
{
    return in->oil_k >= c->oil_opt_k - MF_WARM_OIL_DK
        || e->drain_warm_s >= MF_WARM_FALLBACK_S;
}

static inline int mf_onset_step(MfEngine *e, const MfCar *c, const MfInputs *in)
{
    double life_before = e->life_prev_s;
    double rev_abs = c->rev_limit_rads > 1.0
                   ? in->rpm_rads / c->rev_limit_rads : 0.0;
    int fired = 0;
    int prev_short_run = e->drain_short_run;   /* H2b: survives only if accepted */
    /* R3p (ADDENDUM 11, owner live, manual BLOW UP): while the engine is dead
     * or off no wear/drain accrues. The game's own staged death keeps dropping
     * the life below 0 after our terminal, and the drain EMA used to keep
     * counting (0.17-0.23 s/s at rpm 0) and fed component damage. */
    int engine_dead = (in->life_s <= 0.0 && !in->life_infinite)
                      || in->self_seize || e->blown || e->stopped
                      || !in->engine_running;

    e->drain_short_valid = 0;     /* E9-H2b: only this sample's value counts */
    e->drain_short_run = 0;       /* ... and only a RUN of accepted samples */
    if (e->life_valid && in->life_s < 1e12) {
        double drain_inst = (e->life_prev_s - in->life_s)
                            / (in->dt_s > 1e-6 ? in->dt_s : 1e-6);
        if (in->life_drain_sps > 0.0) {
            /* E9p: the caller knows the clock's own rate (the AI timer ticks
             * 1.0 per game second) - use it instead of re-measuring the life
             * slope, so the life divisor reaches the time-to-fail unchanged */
            e->drain_ema_sps = in->life_drain_sps;
            e->drain_valid = 1;
            mf_dmg_accum(e, in->life_drain_sps * in->dt_s);   /* P1 */
            if (in->on_track && in->engine_running)
                e->drain_warm_s += in->dt_s;
        } else if (!engine_dead && drain_inst >= 0.0
                   && drain_inst <= MF_DRAIN_MAX_SPS) {
            /* E9o: exact zero-order-hold EMA factor so the 60 s time constant
             * holds at any step size (the linear dt/tau form is wrong at the
             * large CTRL+T steps: tau_eff = 56 s at dt = 8 s) */
            e->drain_ema_sps += (drain_inst - e->drain_ema_sps)
                                * (1.0 - exp(-in->dt_s / MF_LIFE_EMA_TAU_S));
            e->drain_valid = 1;
            mf_dmg_accum(e, drain_inst * in->dt_s);           /* P1 */
            if (in->on_track && in->engine_running)
                e->drain_warm_s += in->dt_s;
        } else if (drain_inst > MF_DRAIN_MAX_SPS && isfinite(drain_inst)
                   && !engine_dead
                   && rev_abs > MF_OVR_VT_REV
                   && e->ovr_vt_timer_s > MF_OVR_VT_DWELL_S) {
            /* E9a: a life drop the EMA skips (> MF_DRAIN_MAX_SPS) while the
             * revs sit above MF_OVR_VT_REV is an over-rev jump - the game's
             * own life law just charged the engine for the abuse. It costs
             * valvetrain health (rods half) in proportion to the life lost
             * over the pre-jump life (floor 60 s), and is remembered for the
             * clock-out presentation. Symptoms only - the clock owns deaths. */
            double jump_s = e->life_prev_s - in->life_s;
            double life_denom = e->life_prev_s > 60.0 ? e->life_prev_s : 60.0;
            double dmg = MF_OVR_VT_PER_LIFE * jump_s / life_denom;
            e->overrev_life_s += jump_s;
            if (jump_s > 1.0) e->overrev_events++;
            e->overrev_last_t_s = e->t_s;
            e->h_vt = mf_clamp(e->h_vt - dmg, 0.0, 1.0);
            e->h_rod_dmg += 0.5 * dmg;
            e->h_rod = mf_clamp(1.0 - e->h_rod_dmg, 0.0, 1.0);
        } else if (drain_inst > MF_DRAIN_MAX_SPS && isfinite(drain_inst)
                   && in->life_s > 0.0 && !in->life_infinite
                   && e->life_prev_s > 0.0
                   && in->on_track && in->engine_running
                   && in->reason_code != 4
                   && !e->culminated && !e->blown && !e->stopped) {
            /* E9-H2b (R05/D): a positive-life drop beyond the EMA cap between
             * two adjacent valid samples can be the short-horizon drain
             * estimate. It is accepted only when the car's OWN drain law at
             * this sample is also extreme and the measured slope is
             * consistent with it (0.5x..4x): a debugger/reload jump of
             * hundreds of seconds at low rpm is rejected as a reseed, while
             * the abuse/steep-curve rates the review measured are accepted.
             * Reloads/repairs (life increases) never reach here; collisions
             * (code 4), zero/dead clocks and nonfinite samples are rejected. */
            double law = mf_game_drain(c, in->rpm_rads, in->oil_k, 0.0);
            if (isfinite(law) && law > MF_DRAIN_MAX_SPS * 0.5
                && drain_inst >= 0.5 * law && drain_inst <= 4.0 * law) {
                e->drain_short_sps = drain_inst;
                e->drain_short_valid = 1;
                e->drain_short_run = prev_short_run + 1;   /* consecutive */
            }
        }
        /* E8e: a life increase or a drop beyond MF_DRAIN_MAX_SPS (without the
         * short-horizon evidence above) is a reseed/jump (session edge, game
         * re-arm, debugger) - the sample is skipped and life_prev_s is
         * re-based below, never fed to the EMA */
    } else {
        e->drain_ema_sps = 0.0;
    }

    /* E9a (handoff 4.7 / review item 4): native stage one - the game re-armed
     * its own failure in one sample (life was ~0, now > old + 1 s) while on
     * track. The caller logs it once. No onset yet: fire one now, skipping
     * warm/confirm, anchored on the re-armed life (the normal roll picks the
     * fault - the game's own smoke is the prelude). Onset active: re-base the
     * anchor so the terminal lines up with the new life.
     * E9g-d (addendum 2 section 8): a stint reset zeroes the clock and the
     * game re-seeds it on the next sample, which used to read as a stage one
     * and fired a phantom onset. A genuine stage one never shows life <= 0 on
     * the previous sample, and never follows a counted reset: require
     * life_before > 0 and no reset pending. */
    if (e->life_valid && in->on_track
        && life_before > 0.0 && !e->life_reset_pending
        && life_before < MF_REARM_PREV_S && in->life_s > life_before + 1.0) {
        e->native_stage1 = 1;
        if (!e->onset && !e->culminated && !in->life_infinite
            && e->repair_grace_s <= 0.0) {   /* N5: no onset in the grace */
            int stab = 0;
            int fault = mf_roll_fault(e);
            int sev = mf_roll_severity(e, &stab);
            double delay = -1.0;
            if (stab)
                delay = mf_rand_range(&e->rng, MF_STABILISE_MIN_S,
                                      MF_STABILISE_MIN_S + MF_STABILISE_SPAN_S);
            mf_onset_apply(e, in, fault, sev, delay);  /* anchors to in->life_s */
            fired = 1;
        } else if (e->onset && !e->culminated) {
            e->life_onset_s = in->life_s;
            e->ttf_onset_s = in->life_s / e->drain_onset_sps;   /* E9b item 1 */
        }
    }

    /* E9g-c: culmination is a real clock-out: a drop (previous life > 0) to
     * <= 0 that is consistent with the measured drain (previous life <=
     * max(5 s, 3 x drain x dt + 1 s)), on track, with the engine running on
     * the previous sample. A one-sample drop from a large life - the game
     * zeroes the clock when the car is left / a stint ends - is a reset, not
     * a failure: count it (the caller logs) and re-base only.
     * E9g-e (prompt section 4.5): two more cases. (1) `self_seize`: the DLL
     * itself wrote the zero (our own seizure) - the known terminal: no reset,
     * no reset_pending. (2) a drop under the native collision ReasonCode (4)
     * is the game's OWN wall-crash seizure - a failure (culminated,
     * collision), never a reset (live check 2). */
    /* B6 (ADDENDUM 14): a NURSED AI slot's ai_life zero is not a culmination -
     * the clock has been handed to the driver's nursing (Q2 keeps the game
     * timer owned); only an un-nursed slot lets the clock own the failure. */
    if (in->life_s <= 0.0 && !in->life_infinite && !in->ai_nursed) {
        /* E9g-g: the floor must not lag on steep life curves. The model has
         * the game's own law: use the larger of the measured EMA and the
         * game-law drain of the PREVIOUS sample. A car whose clock drains
         * thousands of s/s at the limiter then culminates, while a garage /
         * stint zero (pit speed, low game drain) stays the E9g-c reset. */
        double drain_ref = e->drain_ema_sps > e->drain_game_prev_sps
                         ? e->drain_ema_sps : e->drain_game_prev_sps;
        double floor_s = 3.0 * drain_ref * in->dt_s + 1.0;
        int collision = (in->reason_code == 4);
        if (floor_s < 5.0) floor_s = 5.0;
        if (life_before > 0.0 && life_before <= floor_s
            && in->on_track && e->eng_prev) {
            e->culminated = 1;                      /* game's clock owns the failure */
            if (collision)
                e->culm_collision = 1;              /* E9g-e: collision, not over-rev */
            else if (e->t_s - e->overrev_last_t_s <= MF_OVR_BLAME_S)
                e->culm_overrev = 1;                /* E9a: present it as a blow-up */
        } else if (life_before > 0.0 && collision) {
            e->culminated = 1;                      /* E9g-e: wall crash is a failure */
            e->culm_collision = 1;
        } else if (life_before > 0.0 && !in->self_seize) {
            e->life_resets++;
            e->life_reset_pending = 1;              /* E9g-d: no stage one after a reset */
        }
    }
    if (in->life_s > 0.0) e->life_reset_pending = 0;   /* E9g-d: any positive sample */
    e->life_prev_s = in->life_s;
    e->life_valid = 1;
    e->eng_prev = in->engine_running;
    /* E9g-g: remember this sample's game-law drain for the NEXT sample's
     * reset floor (the reset rule above already consumed the previous one) */
    e->drain_game_prev_sps = mf_game_drain(c, in->rpm_rads, in->oil_k, 0.0);

    /* R3p: a terminal engine reports zero wear - the drain EMA is frozen at
     * the moment of death and the panel must not show a live-looking rate */
    if (engine_dead && (in->self_seize || e->blown || e->stopped
                        || e->culminated)) {
        e->drain_ema_sps = 0.0;
        e->drain_valid = 0;
    }

    if (!e->onset && !e->culminated && !in->life_infinite
        && e->repair_grace_s <= 0.0                 /* N5: no onset in the grace */
        && in->life_s > 0.0                         /* E9g-c: a zeroed clock
                                                     * (stint reset) is not an onset */
        && in->on_track && in->engine_running && e->drain_valid
        && e->drain_warm_s >= MF_DRAIN_WARM_S
        && e->drain_ema_sps > MF_DRAIN_MIN_SPS) {
        double ttf = in->life_s / e->drain_ema_sps;
        /* 2026-10-06: a cold engine defers the onset (warm engine only), but
         * never past the minimum lead - the clock must not run out with no
         * symptoms first */
        if (ttf < e->lead_s
            && (mf_engine_warm(e, c, in) || ttf <= MF_LEAD_MIN_S)) {
            e->onset_confirm_s += in->dt_s;         /* E8e: below lead, holding */
            if (e->onset_confirm_s >= MF_ONSET_CONFIRM_S) {
                int stab = 0;
                /* Q3: the countdown backstop draws its fault from the damage
                 * history when the hazard model is on (else the old roll);
                 * N3c: an AI onset may draw BLOW-UP first */
                int fault = mf_pick_onset_fault(e, in);
                int sev = mf_roll_severity(e, &stab);
                double delay = -1.0;
                if (stab)
                    delay = mf_rand_range(&e->rng, MF_STABILISE_MIN_S,
                                          MF_STABILISE_MIN_S + MF_STABILISE_SPAN_S);
                mf_onset_apply(e, in, fault, sev, delay);
                fired = 1;
            }
        } else {
            e->onset_confirm_s = 0.0;               /* E8e: below-lead window broke */
        }
    } else if (!e->onset) {
        e->onset_confirm_s = 0.0;
    }

    /* E9-H2b (R05/D): emergency onset. A short-horizon drain that predicts the
     * clock empty before the normal warm(60 s)+confirm(10 s) path could ever
     * fire gets ONE onset now, through the existing fault roll and severity/
     * stabilise draws, bypassing ONLY warm/confirm. The fairness floor is
     * anchored on the short remaining budget; life is never touched and a
     * dead/terminal engine is never re-armed. The felt symptom is immediate
     * (MfOut handling in mf_substep); radio/HUD lines come later. */
    if (!fired && !e->onset && !e->culminated && !e->blown && !e->stopped
        && e->repair_grace_s <= 0.0                 /* N5: emergency path A */
        && e->drain_short_valid && e->drain_short_run >= 2
        && !in->life_infinite
        && in->life_s > 0.0 && in->on_track && in->engine_running) {
        double ttf = in->life_s / e->drain_short_sps;
        double wait = MF_ONSET_CONFIRM_S;
        if (e->drain_warm_s < MF_DRAIN_WARM_S)
            wait += MF_DRAIN_WARM_S - e->drain_warm_s;
        if (isfinite(ttf) && ttf <= wait) {
            int stab = 0;
            /* Q3: same history draw for the emergency (fast) onset;
             * N3c: same AI BLOW-UP first draw */
            int fault = mf_pick_onset_fault(e, in);
            int sev = mf_roll_severity(e, &stab);
            double delay = -1.0;
            if (stab)
                delay = mf_rand_range(&e->rng, MF_STABILISE_MIN_S,
                                      MF_STABILISE_MIN_S + MF_STABILISE_SPAN_S);
            mf_onset_apply(e, in, fault, sev, delay);
            e->drain_onset_sps = e->drain_short_sps > MF_DRAIN_MIN_SPS
                               ? e->drain_short_sps : MF_DRAIN_MIN_SPS;
            e->ttf_onset_s = in->life_s / e->drain_onset_sps;
            e->onset_confirm_s = MF_ONSET_CONFIRM_S;
            e->emergency_onset = 1;
            fired = 1;
        }
    }
    /* T3 (ADDENDUM 12, live all-fail): AI slots with a short written life died
     * "terminal (engine life expired)" with NO onset - the normal path needs
     * warm(60 s) + confirm(10 s), and the caller-known drain branch
     * (life_drain_sps > 0: the AI countdown) sets drain_ema directly, so the
     * short-horizon detector above never sees a sample (drain_short_valid
     * stays 0). One emergency onset now when the KNOWN clock cannot survive
     * the warm+confirm window. */
    if (!fired && !e->onset && !e->culminated && !e->blown && !e->stopped
        && e->repair_grace_s <= 0.0                 /* N5: emergency path B */
        && !in->life_infinite && in->life_s > 0.0 && in->on_track
        && in->engine_running && in->life_drain_sps > 0.0) {
        double ttf = in->life_s / in->life_drain_sps;
        double wait = MF_ONSET_CONFIRM_S;
        if (e->drain_warm_s < MF_DRAIN_WARM_S)
            wait += MF_DRAIN_WARM_S - e->drain_warm_s;
        if (isfinite(ttf) && ttf <= wait) {
            int stab = 0;
            int fault = mf_pick_onset_fault(e, in);
            int sev = mf_roll_severity(e, &stab);
            double delay = -1.0;
            if (stab)
                delay = mf_rand_range(&e->rng, MF_STABILISE_MIN_S,
                                      MF_STABILISE_MIN_S + MF_STABILISE_SPAN_S);
            mf_onset_apply(e, in, fault, sev, delay);
            e->drain_onset_sps = in->life_drain_sps;
            e->ttf_onset_s = ttf;
            e->onset_confirm_s = MF_ONSET_CONFIRM_S;
            e->emergency_onset = 1;
            fired = 1;
        }
    }
    return fired;
}

/* ------------------------------------------------------------------ */
/* E9o: downshift shock (4.0b) on pump samples, in game time.          */
/* The rpm MF_SHIFT_WINDOW_S ago is looked up in a ring of pump        */
/* samples: at fine steps it is the rpm 0.2 s back, at coarse CTRL+T   */
/* steps the previous sample IS the other end of the step. One         */
/* coarse sample therefore can never fake a downshift on its own -     */
/* the jump must exist between two consecutive samples.                */
/* ------------------------------------------------------------------ */

static inline void mf_shift_step(MfEngine *e, const MfCar *c, const MfInputs *in)
{
    int i, best = -1;
    double jump;

    e->shift_rpm[e->shift_idx] = in->rpm_rads;
    e->shift_t_s[e->shift_idx] = e->t_s;
    if (e->shift_n < 32) e->shift_n++;

    if (in->on_track && in->engine_running && !e->culminated
        && !e->blown && !e->stopped) {
        /* newest sample at least MF_SHIFT_WINDOW_S old (scan newest-first) */
        for (i = 1; i <= e->shift_n; i++) {
            int j = (e->shift_idx - i) & 31;
            if (e->t_s - e->shift_t_s[j] >= MF_SHIFT_WINDOW_S) { best = j; break; }
        }
        if (best >= 0) {
            jump = (in->rpm_rads - e->shift_rpm[best]) / c->rev_limit_rads;
            if (jump > MF_SHIFT_JUMP && in->load < MF_SHIFT_LOAD_MAX) {
                e->h_rod_dmg += pow(jump / MF_SHIFT_JUMP, MF_SN_M) / MF_N_SHIFT;
                e->h_rod = mf_clamp(1.0 - e->h_rod_dmg, 0.0, 1.0);
                if (e->film_last < MF_FILM_CRIT)
                    e->h_b = mf_clamp(e->h_b - MF_B_SHOCK * jump, 0.0, 1.0);
            }
        }
    }
    e->shift_idx = (e->shift_idx + 1) & 31;
}

/* ------------------------------------------------------------------ */
/* E9h: contact faults from wall hits.                                 */
/* ------------------------------------------------------------------ */

/* The impact roll: radiator puncture 40 %, exhaust crack 25 %, oil leak
 * 20 %, nothing 15 %. Model rng only; quality never changes these odds
 * (E9h amendment). */
static inline int mf_roll_impact(MfEngine *e)
{
    double r = mf_rng_d(&e->rng);
    if (r < MF_IMPACT_P_WATER) return MF_FAULT_WATER_LEAK;
    if (r < MF_IMPACT_P_WATER + MF_IMPACT_P_EXHAUST) return MF_FAULT_EXHAUST;
    if (r < MF_IMPACT_P_WATER + MF_IMPACT_P_EXHAUST + MF_IMPACT_P_OIL)
        return MF_FAULT_OIL_LEAK;
    return MF_FAULT_NONE;
}

/* Detection (no new address): a speed drop above MF_IMPACT_G over one pump
 * tick of <= MF_IMPACT_MAX_DT_S, or the live-proven collision ReasonCode 4
 * while on track (rising edge; the code-4 hit can carry any step size). One
 * roll per impact while no onset is active and the engine is not already
 * dead; an impact under code 4 with life <= 0 is the game's own native
 * seizure - detected and logged, but no fault (the game already killed it).
 * The fault budget is min(current time-to-fail, MF_IMPACT_BUDGET_MAX_S),
 * floored at MF_IMPACT_BUDGET_MIN_S. This is the only onset the player
 * caused; the E9g-e collision culmination is untouched. */
/* 2026-10-05: fault chance for one contact burst of the given impulse -
 * 0 below MF_IMPACT_IMP_MIN, then linear MIN->MID 0->PC_MID, MID->BIG
 * PC_MID->PC_BIG, BIG->TOP PC_BIG->PC_TOP, flat PC_TOP above. */
static inline double mf_impact_chance(double imp)
{
    if (imp < MF_IMPACT_IMP_MIN) return 0.0;
    if (imp < MF_IMPACT_IMP_MID)
        return MF_IMPACT_PC_MID * (imp - MF_IMPACT_IMP_MIN)
             / (MF_IMPACT_IMP_MID - MF_IMPACT_IMP_MIN);
    if (imp < MF_IMPACT_IMP_BIG)
        return MF_IMPACT_PC_MID + (MF_IMPACT_PC_BIG - MF_IMPACT_PC_MID)
             * (imp - MF_IMPACT_IMP_MID) / (MF_IMPACT_IMP_BIG - MF_IMPACT_IMP_MID);
    if (imp < MF_IMPACT_IMP_TOP)
        return MF_IMPACT_PC_BIG + (MF_IMPACT_PC_TOP - MF_IMPACT_PC_BIG)
             * (imp - MF_IMPACT_IMP_BIG) / (MF_IMPACT_IMP_TOP - MF_IMPACT_IMP_BIG);
    return MF_IMPACT_PC_TOP;
}

/* Fault type given that a fault happens: the legacy 40/25/20 split,
 * renormalised (water 47 %, exhaust 29 %, oil 24 %). */
static inline int mf_roll_impact_type(MfEngine *e)
{
    double tot = MF_IMPACT_P_WATER + MF_IMPACT_P_EXHAUST + MF_IMPACT_P_OIL;
    double r = mf_rng_d(&e->rng) * tot;
    if (r < MF_IMPACT_P_WATER) return MF_FAULT_WATER_LEAK;
    if (r < MF_IMPACT_P_WATER + MF_IMPACT_P_EXHAUST) return MF_FAULT_EXHAUST;
    return MF_FAULT_OIL_LEAK;
}

/* Impulse-source impact (player). One call per contact burst; no latch (the
 * DLL already merges a burst into one peak). Pit-return teleports carry no
 * contact, so they never reach here. */
static inline void mf_impact_impulse_step(MfEngine *e, const MfInputs *in,
                                          MfOut *out)
{
    double imp = in->impulse;
    double pc;
    if (!in->on_track || in->life_infinite || imp < MF_IMPACT_IMP_MIN) return;
    pc = mf_impact_chance(imp);
    out->impact_fired = 1;
    out->impact_g = -1.0;
    out->impact_impulse = imp;
    out->impact_chance = pc;
    out->impact_fault = MF_FAULT_NONE;
    if (e->onset || e->culminated || e->blown || e->stopped
        || !(in->life_s > 0.0))
        return;
    if (mf_rng_d(&e->rng) < pc) {
        int sev = imp < MF_IMPACT_IMP_MID ? MF_SEV_SMALL
                : imp < MF_IMPACT_IMP_BIG ? MF_SEV_MEDIUM : MF_SEV_BIG;
        int fault = mf_roll_impact_type(e);
        double ttf = (e->drain_valid && e->drain_ema_sps > MF_DRAIN_MIN_SPS)
                   ? in->life_s / e->drain_ema_sps : MF_IMPACT_BUDGET_MAX_S;
        double budget = ttf < MF_IMPACT_BUDGET_MAX_S ? ttf : MF_IMPACT_BUDGET_MAX_S;
        if (budget < MF_IMPACT_BUDGET_MIN_S) budget = MF_IMPACT_BUDGET_MIN_S;
        mf_arm_budget(e, in, fault, sev, budget, -1.0);
        out->impact_fault = fault;
    }
}

static inline void mf_impact_step(MfEngine *e, const MfCar *c, const MfInputs *in,
                                  MfOut *out)
{
    int code4 = in->on_track && in->reason_code == 4;
    double g = -1.0;
    int detected = 0;

    (void)c;
    if (in->impulse_src) {
        mf_impact_impulse_step(e, in, out);
        e->impact_prev_valid = 0;
        e->impact_latch = 0;
        e->impact_prev_code4 = code4;
        return;
    }
    if (in->on_track && !in->life_infinite && !e->impact_latch) {
        if (code4 && !e->impact_prev_code4) {
            detected = 1;
        } else if (e->impact_prev_valid && in->dt_s > 0.0
                   && in->dt_s <= MF_IMPACT_MAX_DT_S) {
            double dg = (e->impact_prev_speed - in->speed_mps) / (in->dt_s * 9.81);
            if (dg > MF_IMPACT_G) { detected = 1; g = dg; }
        }
    }

    if (detected) {
        e->impact_latch = 1;
        if (g < 0.0 && e->impact_prev_valid && in->dt_s > 0.0
            && in->dt_s <= MF_IMPACT_MAX_DT_S)
            g = (e->impact_prev_speed - in->speed_mps) / (in->dt_s * 9.81);
        if (g < MF_IMPACT_G) g = MF_IMPACT_G;   /* code-4 hit, no usable drop */
        out->impact_fired = 1;
        out->impact_g = g;
        out->impact_fault = MF_FAULT_NONE;
        if (!e->onset && !e->culminated && !e->blown && !e->stopped
            && in->life_s > 0.0) {
            int sev = g < MF_IMPACT_SEV_SMALL_G ? MF_SEV_SMALL
                    : g <= MF_IMPACT_SEV_BIG_G ? MF_SEV_MEDIUM : MF_SEV_BIG;
            int fault = mf_roll_impact(e);
            if (fault != MF_FAULT_NONE) {
                double ttf = (e->drain_valid && e->drain_ema_sps > MF_DRAIN_MIN_SPS)
                           ? in->life_s / e->drain_ema_sps : MF_IMPACT_BUDGET_MAX_S;
                double budget = ttf < MF_IMPACT_BUDGET_MAX_S ? ttf
                                                             : MF_IMPACT_BUDGET_MAX_S;
                if (budget < MF_IMPACT_BUDGET_MIN_S) budget = MF_IMPACT_BUDGET_MIN_S;
                mf_arm_budget(e, in, fault, sev, budget, -1.0);
                out->impact_fault = fault;
            }
        }
    }

    /* latch release: quiet again (no code 4 and the last drop is small) */
    if (e->impact_latch) {
        double dg = 0.0;
        if (e->impact_prev_valid && in->dt_s > 0.0
            && in->dt_s <= MF_IMPACT_MAX_DT_S)
            dg = (e->impact_prev_speed - in->speed_mps) / (in->dt_s * 9.81);
        if (!code4 && dg < MF_IMPACT_G * 0.5)
            e->impact_latch = 0;
    }

    e->impact_prev_speed = in->speed_mps;
    e->impact_prev_valid = 1;
    e->impact_prev_code4 = code4;
}

/* ------------------------------------------------------------------ */
/* Internal physics step (fixed MF_MODEL_DT)                           */
/* ------------------------------------------------------------------ */

static inline double mf_phys_oil(const MfCar *c, double w_oil)
{
    double r = c->w_oil_ref > 1e-9 ? w_oil / c->w_oil_ref : 1.0;
    r = mf_clamp(r, 0.02, 8.0);
    return pow(r, 1.5);      /* hot-oil wear is strongly nonlinear (E1b) */
}

static inline void mf_out_default(MfOut *o)
{
    memset(o, 0, sizeof *o);
    o->power_mult = 1.0;
    o->vfuel_press = -1.0;
    o->vfuel_valve = -1.0;
    o->rev_ceiling = -1.0;
    o->rad_scale = 1.0;
    o->rad_air_scale = 1.0;
    o->oil_gauge_scale = 1.0;      /* E9d: healthy gauge */
    o->impact_impulse = -1.0;
}

/* 2026-10-05: the car's warm water baseline, never below
 * MF_WATER_BASE_MIN_K (a cold engine is not the reference for steam,
 * warnings or the system pressure). */
static inline double mf_water_base(const MfEngine *e, const MfInputs *in)
{
    double wb = e->water_base_k > 0.0 ? e->water_base_k : in->water_k;
    return wb > MF_WATER_BASE_MIN_K ? wb : MF_WATER_BASE_MIN_K;
}

static inline void mf_substep(MfEngine *e, const MfCar *c, const MfInputs *in, MfOut *out)
{
    double dt = MF_MODEL_DT;
    double rev, w_rpm, w_oil, load, q, g_comb, ambient_k, t_bay;
    double grow = e->stabilised ? 0.0 : 1.0;        /* 1.3.7: stop growing */
    double clock_drive, growth_scale, mu_oil, film, bl, sev_g;
    int active, fair;

    e->t_s += dt;

    /* E9d item 4: the lean shortfall is only valid while the fuel-fault
     * physics ran (it feeds the exhaust heat and the extra burn); clear it
     * otherwise so a finished fault cannot keep leaning the mixture. */
    if (!(e->onset && !e->culminated && !e->blown && !e->stopped
          && (e->fault == MF_FAULT_FUEL_PRESSURE
              || e->fault == MF_FAULT_FUEL_LEAK)))
        e->lean_shortfall = 0.0;

    /* E9b-2 item 6: early random failure (bathtub curve). One hazard roll per
     * whole game second while the engine runs on track with no onset and no
     * terminal; latched once per engine (early_done until model_reset).
     * severity mild or medium only; budget uniform 300-900 s; stabilise chance
     * as today; never in the pits or off track. */
    if (in->early_enable && !in->life_infinite && in->on_track && in->engine_running
        && e->repair_grace_s <= 0.0                  /* N5: no early onset in the grace */
        && mf_engine_warm(e, c, in)                  /* 2026-10-06: warm engine only */
        && !in->in_pit && !e->onset && !e->culminated && !e->blown && !e->stopped
        && !e->early_done) {
        e->early_acc_s += dt;
        if (e->early_acc_s >= 1.0) {
            e->early_acc_s -= 1.0;
            if (mf_early_roll(e, e->drain_ema_sps, in->save_mode)) {
                int sev = mf_rng_d(&e->rng) < MF_SEV_P_SMALL
                        ? MF_SEV_SMALL : MF_SEV_MEDIUM;   /* never severe */
                double budget = mf_rand_range(&e->rng, MF_EARLY_BUDGET_MIN_S,
                                              MF_EARLY_BUDGET_MAX_S);
                double delay = -1.0;
                if (mf_rng_d(&e->rng) < MF_STABILISE_P)
                    delay = mf_rand_range(&e->rng, MF_STABILISE_MIN_S,
                                          MF_STABILISE_MIN_S + MF_STABILISE_SPAN_S);
                mf_arm_budget(e, in, mf_roll_fault(e), sev, budget, delay);
                e->early_done = 1;
                out->early_fired = 1;
            }
        }
    } else {
        e->early_acc_s = 0.0;
    }

    /* 4.0 shared derived */
    rev = in->rpm_rads / c->rev_limit_rads;
    w_rpm = mf_pow2((in->rpm_rads - c->rpm_base_rads) / c->rpm_range_rads);
    w_oil = mf_pow2((in->oil_k - c->oil_base_k) / c->oil_range_k);
    load = mf_clamp(in->load, 0.0, 1.0);
    q = 0.5 * MF_AIR_DENSITY * in->speed_mps * in->speed_mps;
    g_comb = sqrt(in->g_long * in->g_long + in->g_lat * in->g_lat);
    ambient_k = in->ambient_c + 273.15;
    t_bay = 0.5 * (in->oil_k + in->water_k);

    /* fuel flow (2 s average, refuel jumps ignored) */
    if (e->fuel_prev_l > 0.0 && in->fuel_l <= e->fuel_prev_l && !in->in_pit) {
        double inst = (e->fuel_prev_l - in->fuel_l) / dt;
        e->fuel_flow_lps += (inst - e->fuel_flow_lps)
                            * mf_clamp(dt / MF_FUEL_EMA_TAU_S, 0.0, 1.0);
    }
    e->fuel_prev_l = in->fuel_l;
    if (e->fuel_flow_lps > e->fuel_flow_peak_lps)
        e->fuel_flow_peak_lps = e->fuel_flow_lps;

    /* 4.0 exhaust gas temperature (first order, tau 2 s; E8g item 7: with the
     * engine off the target is ambient and the gas cools toward it; E9b item
     * 4: the rise carries a rev factor 0.6 + 0.4 x rev) */
    {
        double tgt = in->engine_running
                   ? ambient_k + MF_EXH_RISE_K * pow(load > 0.0 ? load : 0.0, 0.8)
                     * (0.6 + 0.4 * mf_clamp(rev, 0.0, 1.5))
                     + MF_EXH_LEAN_K * e->lean_shortfall
                   : ambient_k;
        e->t_exh_k += (tgt - e->t_exh_k) * mf_clamp(dt / MF_EXH_TAU_S, 0.0, 1.0);
    }
    /* 4.0a component temperatures (E8g item 7: every target is clamped to
     * >= ambient_k and with the engine off all targets = ambient) */
    {
        double tgt;
        int eng_on = in->engine_running;
        tgt = eng_on ? in->oil_k + MF_PIS_RISE_K * load * rev : ambient_k;
        if (tgt < ambient_k) tgt = ambient_k;
        e->t_pis_k += (tgt - e->t_pis_k) * mf_clamp(dt / MF_PIS_TAU_S, 0.0, 1.0);
        tgt = eng_on ? e->t_exh_k - MF_EV_DROP_K : ambient_k;
        if (tgt < ambient_k) tgt = ambient_k;
        e->t_ev_k += (tgt - e->t_ev_k) * mf_clamp(dt / MF_EV_TAU_S, 0.0, 1.0);
        bl = 0.3 + 0.7 * (load * 0.6 + rev * rev * 0.4);
        /* friction feedback is bounded: a dry bearing is hot, not unbounded */
        tgt = eng_on ? in->oil_k + MF_BRG_RISE_K * rev * rev
                       * mf_clamp(1.5 - e->film_last, 0.5, 1.0)
                     : ambient_k;
        if (tgt < ambient_k) tgt = ambient_k;
        e->t_brg_k += (tgt - e->t_brg_k) * mf_clamp(dt / MF_BRG_TAU_S, 0.0, 1.0);
        tgt = eng_on ? in->water_k + MF_HEAD_RISE_K * load : ambient_k;
        if (tgt < ambient_k) tgt = ambient_k;
        e->t_head_k += (tgt - e->t_head_k) * mf_clamp(dt / MF_HEAD_TAU_S, 0.0, 1.0);
    }

    if (e->onset && (e->fault == MF_FAULT_OIL_PRESSURE || e->fault == MF_FAULT_OIL_LEAK)
        && e->oil_base_k < 0.0) e->oil_base_k = (-e->oil_base_k > c->oil_opt_k ? -e->oil_base_k : c->oil_opt_k);

    /* E8g item 2: the car's own water baseline (EMA, tau 120 s) while no
     * cooling fault is active and on track; frozen once one is. */
    {
        int cool_fault = e->onset && (e->fault == MF_FAULT_WATER_LEAK
                                      || e->fault == MF_FAULT_RAD_DEBRIS);
        /* 2026-10-05: learn only from a warm engine - live, a pit-out cold
         * baseline (~30 C) gave thick steam and COOL THE ENGINE at 44 C */
        /* 2026-10-05 b: 'warm' = the oil at operating temperature (the game
         * thermostat holds opt - 10 K), not water >= 70 C - a car whose water
         * runs 65 C learns its true normal instead of sitting on the floor */
        if (!cool_fault && in->on_track && in->oil_k >= c->oil_opt_k - 15.0) {
            /* 2026-10-05 c (live Indy, healthy car): water 50 -> 90 C over two
             * laps while the EMA baseline sat at 52 C - any fault would have
             * warned at once. The normal now follows a rising (warming) water
             * at once and falls back slowly (tau 120 s). */
            if (e->water_base_k <= 0.0 || in->water_k > e->water_base_k)
                e->water_base_k = in->water_k;
            else e->water_base_k += (in->water_k - e->water_base_k)
                                    * mf_clamp(dt / MF_WATER_BASE_TAU_S, 0.0, 1.0);
        }
        /* 2026-10-05 c: the oil normal, same rule (up at once, down tau 120 s),
         * frozen while an oil fault is active */
        {
            int oil_fault = e->onset && (e->fault == MF_FAULT_OIL_PRESSURE
                                         || e->fault == MF_FAULT_OIL_LEAK);
            if (!oil_fault && in->on_track && in->oil_k >= c->oil_opt_k - 15.0) {
                if (e->oil_base_k <= 0.0 || in->oil_k > e->oil_base_k)
                    e->oil_base_k = in->oil_k;
                else e->oil_base_k += (in->oil_k - e->oil_base_k)
                                      * mf_clamp(dt / MF_WATER_BASE_TAU_S, 0.0, 1.0);
            }
        }
    }

    /* 1.2 stress shares (last 600 s), excluding pit time.
     * 2026-10-09 share-units fix: every stressor is a dimensionless ratio
     * (rev: game drain weight, 1 at rpm_base; oil: 1 at oil_opt; water:
     * excess / MF_SHARE_WATER_REF_K; thermal: exhaust swing off its
     * running mean / MF_SHARE_THERM_REF_K; g: excess / MF_SHARE_G_REF).
     * Before, the thermal term was |t_exh - midpoint(peak, trough)| with
     * peak/trough only tracked inside an ACTIVE exhaust fault - so before
     * any onset it was the raw exhaust temperature above the 298 K seed
     * (hundreds of K) and pinned the fault mix at thermal 100 %. */
    if (!in->in_pit && in->engine_running) {
        double decay = dt / MF_SHARE_TAU_S;
        double s_rev = w_rpm;
        double s_oil = c->w_oil_ref > 0.0 ? w_oil / c->w_oil_ref : 0.0;
        e->share_rev     += (s_rev - e->share_rev) * decay
                            + s_rev * dt * (1.0 - decay);
        e->share_oil     += (s_oil - e->share_oil) * decay
                            + s_oil * dt * (1.0 - decay);
        {
            double wh = in->water_k > c->oil_opt_k
                      ? (in->water_k - c->oil_opt_k) / MF_SHARE_WATER_REF_K : 0.0;
            e->share_water += (wh - e->share_water) * decay
                              + wh * dt * (1.0 - decay);
        }
        {
            double dsw;
            if (e->t_exh_avg_k <= 0.0) e->t_exh_avg_k = e->t_exh_k;
            e->t_exh_avg_k += (e->t_exh_k - e->t_exh_avg_k)
                              * mf_clamp(dt / MF_SHARE_EXH_AVG_TAU_S, 0.0, 1.0);
            dsw = fabs(e->t_exh_k - e->t_exh_avg_k) / MF_SHARE_THERM_REF_K;
            e->share_thermal += (dsw - e->share_thermal) * decay
                                + dsw * dt * (1.0 - decay);
        }
        {
            double gg = g_comb > 2.0 ? (g_comb - 2.0) / MF_SHARE_G_REF : 0.0;
            e->share_g += (gg - e->share_g) * decay + gg * dt * (1.0 - decay);
        }
    }

    /* 4.7 conrods / over-rev: always on while on track and running (E6 gate) */
    /* 2026-10-06 owner (vanilla+): Failure Rate 'none' = no mod-made failure;
     * the game has no over-rev damage of its own, so this layer is off too. */
    active = in->on_track && in->engine_running && !e->culminated && !e->blown && !e->stopped
             && !in->life_infinite;
    if (active) {
        double rev_abs = rev;
        /* E9a: the game's own torque curve owns this death - above the car's
         * torque-zero rpm (RPMTorque table; fallback MF_TQ_ZERO_FALLBACK x
         * limit) for MF_OVR_HARD_S the valve hits the piston. The only model
         * death not tied to an onset: it is driver error, so it is exempt
         * from the E9b fairness floor. */
        if (!in->ai_mode && in->rpm_rads > c->tq_zero_rads) {
            /* E9o: two consecutive samples above the threshold; one coarse
             * CTRL+T sample is not evidence of 0.3 s of dwell */
            if (e->overrev_prev_above) e->overrev_timer_s += dt;
            if (e->overrev_timer_s > MF_OVR_HARD_S) {
                e->blown = 1;
                snprintf(out->reason, sizeof out->reason, "over-rev: valve hit piston");
            }
        } else {
            e->overrev_timer_s = 0.0;
        }
        /* Miner fatigue above the endurance limit. 2026-10-07 (live: AI
         * Montoya, 15 g crash -> "conrod fatigue" blow-up 15 s later - a new
         * rod needs ~1.8x the limit sustained for that, i.e. a garbage rpm
         * sample after the crash): the fatigue rev is capped at the worst
         * real limiter overshoot, so fatigue alone needs hours, never seconds.
         * Real over-rev deaths keep their own paths (torque-zero, valves). */
        {
            double rev_f = rev_abs < MF_FATIGUE_REV_CAP ? rev_abs : MF_FATIGUE_REV_CAP;
            double sigma = rev_f * rev_f;
            if (sigma > MF_SIGMA_E) {
                double rpm_f = rev_abs > 1e-9 ? in->rpm_rads * (rev_f / rev_abs) : in->rpm_rads;
                double revs = (rpm_f / (2.0 * 3.14159265358979323846)) * dt;
                e->h_rod_dmg += pow(sigma / MF_SIGMA_REF, MF_SN_M) / MF_N_REF * revs;
                e->h_rod = mf_clamp(1.0 - e->h_rod_dmg, 0.0, 1.0);
                if (e->h_rod <= 0.0) {
                    e->blown = 1;
                    snprintf(out->reason, sizeof out->reason, "conrod fatigue");
                }
            }
        }
        /* valve return over-rev: valvetrain damage above 1.06 (E9b: raised from
         * 1.02); the rpm must dwell above it for MF_OVR_VT_DWELL_S first, so
         * a short downshift blip costs nothing. Never touches the fault. */
        if (rev_abs > MF_OVR_VT_REV) {
            e->ovr_vt_timer_s += dt;
            if (e->ovr_vt_timer_s > MF_OVR_VT_DWELL_S) {
                double d = rev_abs - MF_OVR_VT_REV;
                e->h_vt = mf_clamp(e->h_vt - d * d * MF_OVR_VT_K * dt, 0.0, 1.0);
            }
        } else {
            e->ovr_vt_timer_s = 0.0;
        }
        /* engine braking (4.0b); the downshift shock itself is mf_shift_step
         * (E9o: per pump sample, 0.2 s window in game time) */
        if (load < MF_EB_LOAD_MAX && rev > MF_EB_REV_MIN) {
            e->h_vt = mf_clamp(e->h_vt - 0.3 * MF_EB_K * rev * rev * dt, 0.0, 1.0);
            e->h_b  = mf_clamp(e->h_b  - 0.3 * MF_EB_K * rev * rev * dt, 0.0, 1.0);
        }
    }

    /* fault physics: only after onset, before culmination/termination */
    if (!e->onset || e->culminated || e->blown || e->stopped) {
        goto outputs;
    }

    /* 2026-10-07 audit MF-04: owner containment - an existing fault freezes
     * off circuit (pit lane, box, garage, menus) and carries on after. */
    if (!in->on_track) {
        goto outputs;
    }

    /* N3b (owner D3): BLOW-UP has no component physics - the rolled warning
     * runs (rough running), then the engine blows. Deliberately BEFORE the
     * fairness-floor computation: the owner ruled the floor must not delay
     * this terminal beyond the warning. */
    if (e->fault == MF_FAULT_BLOWUP) {
        if (e->t_s - e->onset_t_s >= e->blowup_warn_s) {
            e->blown = 1;
            snprintf(out->reason, sizeof out->reason, "engine blow-up");
        }
        goto outputs;
    }

    if (e->stop_delay_s >= 0.0 && !e->stabilised
        && e->t_s - e->onset_t_s >= e->stop_delay_s)
        e->stabilised = 1;

    /* E9b-2 item 8 / E9q / E9i: fault growth rate x 1/(q x sqrt(G)), and
     * x MF_SAVE_GROWTH while save mode is on (zeroed quality/grade read 1) */
    /* 2026-10-05: a virtual-budget arm re-anchors on the settled drain (one
     * EMA time constant after the arm, on track) and, until then and after,
     * develops at most MF_ARM_DRIVE_MAX x its anchored pace - an arm at
     * pit-out (drain ~0) no longer runs 12-300x fast at race pace. */
    if (e->arm_virtual && e->arm_reanchor && in->on_track
        && e->t_s - e->onset_t_s >= MF_LIFE_EMA_TAU_S
        && e->drain_ema_sps > MF_DRAIN_MIN_SPS) {
        e->drain_onset_sps = e->drain_ema_sps;
        e->life_onset_s = e->arm_budget_s * e->drain_ema_sps / e->arm_gain;
        e->arm_reanchor = 0;
    }
    clock_drive = (e->arm_virtual
                   ? mf_clamp(e->drain_ema_sps / e->drain_onset_sps, 0.0,
                              MF_ARM_DRIVE_MAX)
                     * e->drain_onset_sps
                   : e->drain_ema_sps)
                  / mf_clamp(e->life_onset_s, 1.0, 1e9)
                  / mf_clamp(e->quality > 1e-6 ? e->quality : 1.0, 0.5, 2.0)
                  / sqrt(mf_clamp(e->grade > 1e-6 ? e->grade : 1.0,
                                  MF_GRADE_MIN, MF_GRADE_MAX))
                  * (in->save_mode ? MF_SAVE_GROWTH : 1.0);
    /* E9-H2d (R10/D3): the SAME intended growth scale (save x0.6, quality,
     * grade) for the physical-rate progression paths that do not run through
     * clock_drive: head-gasket thermal creep and cascade, and the exhaust
     * crack (Paris + vibration). One scale, every progression path. */
    growth_scale = (in->save_mode ? MF_SAVE_GROWTH : 1.0);
    growth_scale /= mf_clamp(e->quality > 1e-6 ? e->quality : 1.0, 0.5, 2.0);
    growth_scale /= sqrt(mf_clamp(e->grade > 1e-6 ? e->grade : 1.0,
                                  MF_GRADE_MIN, MF_GRADE_MAX));
    /* E9-H2a (R04): the fairness floor must gate every model terminal, so the
     * terminal checks below see it BEFORE they latch. Driver-error terminals
     * (the E9a torque-zero death, conrod Miner fatigue, the fuel fire with its
     * own gate) are explicit exceptions and stay outside this window. */
    fair = e->t_s < e->onset_t_s + MF_FAIR_FRAC * e->ttf_onset_s;
    /* Claude review of run 4 (2026-10-01): on the AI countdown the vanilla
     * clock IS the WHEN (owner fairness rule). Q7a caps the budget at the
     * remaining life, so near the end clock_drive grows without bound and a
     * fast fault (T2 water leak: q6 variant 1 row 28 seed 172, gasket stop
     * 57 s before a 5432 s timer) could end the engine before the clock.
     * Hold every component at its fairness floor until the AI clock is out;
     * the Q7a hard terminal then ends it on time. Player and AI hazard runs
     * unchanged. */
    /* B2/B5 (ADDENDUM 14): with the reaction layer live the AI runs the SAME
     * damage physics as the player - the blanket "hold everything until the
     * clock" override stays only on the legacy path (ai_reaction 0), so a
     * mistake (M1/M2) can really blow the engine and nursing really slows it. */
    if (in->ai_mode && !in->onset_model && !in->life_infinite
        && in->life_s > 0.0 && !in->ai_reaction)
        fair = 1;
    sev_g = mf_sev_gain(e->severity);

    switch (e->fault) {

    /* 4.1 valve return force ----------------------------------------- */
    case MF_FAULT_VALVE_AIR: {
        double ratio = t_bay / MF_T_BAY_REF_K;
        double p = e->valve_m * ratio;
        double dmdt = -e->valve_a * p / sqrt(ratio) - MF_VALVE_USE_K * rev * p;
        /* R3a: severity plateau - the leak area stops growing once p has
         * reached the floor and p itself is floored (see the constants). */
        double p_floor = (e->severity == MF_SEV_SMALL ? MF_VALVE_CEIL_FLOOR_SMALL
                          : e->severity == MF_SEV_BIG ? MF_VALVE_CEIL_FLOOR_BIG
                          : MF_VALVE_CEIL_FLOOR_MED);
        p_floor *= p_floor;
        if (p > p_floor)
            e->valve_a += MF_VALVE_LEAK_GROW * sev_g * clock_drive * grow * pow(rev / MF_REV_REF, 2.0) * dt;
        e->valve_m = mf_clamp(e->valve_m + dmdt * dt, 0.0, 1.5);
        p = e->valve_m * ratio;
        if (p < p_floor) {                          /* R3a: limp-home floor */
            p = p_floor;
            if (ratio > 1e-9) e->valve_m = p_floor / ratio;
        }
        /* E9e item 2/7: low valve-return pressure costs high-rpm power; the
         * hook applies it through the smoothstep shape above peak_tq_rads */
        out->loss_top = mf_clamp(1.0 - p, 0.0, 1.0);
        {
            double ceiling = sqrt(mf_clamp(p, 0.0, 4.0) / 1.0);
            if (ceiling < MF_VALVE_CEIL_MIN) ceiling = MF_VALVE_CEIL_MIN;
            if (ceiling > MF_VALVE_CEIL_MAX) ceiling = MF_VALVE_CEIL_MAX;
            out->rev_ceiling = ceiling;
            if (rev > ceiling) {                    /* valve float: rev above the ceiling */
                double over = rev / ceiling - 1.0;
                /* B2 (ADDENDUM 14, spec 4.1): the AI now runs the SAME float
                 * damage as the player. The reaction layer keeps the AI under
                 * the ceiling (rev cap); only perception error, pre-notice
                 * float and M1/M2 mistakes put it over, so a mistake can
                 * really wreck the valvetrain (owner direction 2026-10-01 g).
                 * The legacy path (ai_reaction 0) is bypassed by the caller. */
                e->h_vt = mf_clamp(e->h_vt - MF_VALVE_FLOAT_K * over * over * rev * dt, 0.0, 1.0);
                out->warn_valve_float = 1;
                out->vfuel_valve = 0.5;             /* misfire through the valve lever */
            }
        }
        out->warn_valve_air_low = p < MF_VALVE_P_LOW;
        out->warn_pit_air = p < MF_VALVE_P_PIT;
        out->warn_revs_down = (rev > mf_clamp(out->rev_ceiling, 0.0, 1.0));
        out->warn_shift_earlier |= e->h_vt < MF_VT_SHIFT_EARLY;
        if (!fair && e->h_vt <= 0.0) {              /* H2a: floor first */
            e->blown = 1;
            snprintf(out->reason, sizeof out->reason, "valve hits piston");
        }
        break;
    }

    /* 4.2 lubrication: pump wear, leak, bearing film ---------------- */
    case MF_FAULT_OIL_PRESSURE:
    case MF_FAULT_OIL_LEAK: {
        double phys_oil, p_oil, pickup = 1.0;
        /* E9f: the film calibration runs on an effective temperature derived
         * from the (now physical, 35 K) bearing rise, so every E8g lubrication
         * target keeps the calibration the old 300 K rise produced. */
        double t_visc = in->oil_k + MF_FILM_TEMP_GAIN * (e->t_brg_k - in->oil_k);
        mu_oil = mf_mu(t_visc, c);
        bl = 0.3 + 0.7 * (load * 0.6 + rev * rev * 0.4);
        if (g_comb >= 12.0 * (e->v_oil_l / MF_OIL_START_L))
            pickup = 0.3;                            /* slosh starvation (4.2) */
        phys_oil = mf_phys_oil(c, w_oil);
        if (e->fault == MF_FAULT_OIL_PRESSURE) {
            e->pump_eta -= MF_PUMP_K * sev_g * clock_drive * phys_oil * grow * dt;
            if (e->pump_eta < MF_PUMP_FLOOR) e->pump_eta = MF_PUMP_FLOOR;
        }
        p_oil = e->pump_eta * (rev < 0.55 ? rev / 0.55 : 1.0)
                * sqrt(mu_oil > 0.0 ? mu_oil : 0.0) * pickup;
        if (p_oil > 1.0) p_oil = 1.0;               /* relief valve */
        /* E9d item 1: the native cockpit gauge follows p_oil / p_oil_healthy
         * while a lubrication fault is active (full pump efficiency, no
         * pickup starvation in the denominator); the slosh flicker is in
         * p_oil, so the needle flickers with it */
        {
            double p_healthy = (rev < 0.55 ? rev / 0.55 : 1.0)
                               * sqrt(mu_oil > 0.0 ? mu_oil : 0.0);
            if (p_healthy > 1.0) p_healthy = 1.0;
            out->oil_gauge_scale = p_healthy > 1e-6
                ? mf_clamp(p_oil / p_healthy, 0.0, 1.0) : 1.0;
        }
        if (e->fault == MF_FAULT_OIL_LEAK) {
            e->oil_a += MF_OIL_A_GROW * sev_g * clock_drive * grow * pow(rev / MF_REV_REF, 2.0) * dt;
            e->v_oil_l = mf_clamp(e->v_oil_l
                         - MF_OIL_LEAK_K * e->oil_a * sqrt(p_oil)
                           * clock_drive * sev_g * grow * dt,
                         0.0, MF_OIL_START_L);
        }
        /* blow-by crankcase loss is handled in the rings case */
        film = p_oil * mu_oil * rev / bl;
        e->film_last = film;
        if (film < MF_FILM_CRIT) {
            double arch = (MF_FILM_CRIT - film) * bl * (rev / MF_REV_REF)
                          / (MF_FILM_CRIT * 0.71);
            e->h_b = mf_clamp(e->h_b - MF_BEARING_K * sev_g * clock_drive * arch * grow * dt, 0.0, 1.0);
            e->wear_active_s += dt;
            out->warn_oil_pressure = 1;
        } else {
            e->wear_active_s = 0.0;
        }
        /* E9b-2 item 7: oil-leak cascade - once the sump is below 60 %, the
         * starved pickup wears the bearings as in the oil-pressure fault at
         * half rate: the Archard rate scaled by the deficit below the
         * threshold (0 at 60 %, 1 at dry). Part damage only, no second fault
         * and no second terminal. */
        if (e->fault == MF_FAULT_OIL_LEAK
            && e->v_oil_l < MF_CASCADE_OIL_FRAC * MF_OIL_START_L) {
            double deficit = (MF_CASCADE_OIL_FRAC * MF_OIL_START_L - e->v_oil_l)
                             / (MF_CASCADE_OIL_FRAC * MF_OIL_START_L);
            double arch = mf_clamp(deficit, 0.0, 1.0);
            e->h_b = mf_clamp(e->h_b - MF_CASCADE_WEAR_K * MF_BEARING_K * sev_g
                              * clock_drive * arch * grow * dt, 0.0, 1.0);
            out->cascade_kind = 1;
        }
        if (e->wear_active_s > 3.0) out->warn_shift_earlier = 1;
        if (in->oil_k > c->oil_opt_k + MF_COOL_OIL_MARGIN_K
            && (e->oil_base_k <= 0.0 || in->oil_k > e->oil_base_k + MF_OIL_WARN_DK))
            out->warn_cool_oil = 1;
        /* R2a (ADDENDUM 10, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): the white
         * bank1 plume is gone - the DLL emits the legacy BLUE smoke scaled
         * by oil LOST (emit_bluesmoke_burst, oil_pump). */
        out->oil_heat_ks = (1.0 - e->v_oil_l / MF_OIL_START_L) * MF_OIL_HEAT_MAX_KPS * rev
                           + (1.0 - e->h_b) * MF_BEARING_HEAT_KPS;
        if (!fair && e->h_b <= 0.0) {               /* H2a: floor first */
            e->blown = 1;
            snprintf(out->reason, sizeof out->reason, "bearing seizure");
        }
        break;
    }

    /* 4.3 cooling: debris, water leak, head gasket ------------------- */
    case MF_FAULT_RAD_DEBRIS:
    case MF_FAULT_WATER_LEAK: {
        double p_sys;
        if (e->fault == MF_FAULT_RAD_DEBRIS) {
            e->debris_clear_acc_s += dt;
            if (e->debris_clear_acc_s >= 1.0) {           /* one roll per second */
                double q_ref = 0.5 * MF_AIR_DENSITY * 60.0 * 60.0;
                double pclr = MF_DEBRIS_CLEAR_P * (q_ref > 1.0 ? q / q_ref : 1.0);
                e->debris_clear_acc_s -= 1.0;
                if (mf_rng_d(&e->rng) < pclr) e->f_rad = 1.0;   /* debris falls off */
            }
        }
        if (e->fault == MF_FAULT_WATER_LEAK) {
            /* E8g item 2: pressurised system, 1 bar cold to the cap as the
             * engine warms toward the car's own baseline; above the boiling
             * point at that pressure the vapour term takes over (material) */
            {
                double t_cold = ambient_k;
                double t_run = mf_water_base(e, in);
                double frac;
                if (t_run < ambient_k + 30.0) t_run = ambient_k + 30.0;
                frac = (in->water_k - t_cold) / (t_run - t_cold);
                p_sys = 1.0 + (MF_CAP_BAR - 1.0) * mf_clamp(frac, 0.0, 1.0);
                {
                    double p_vap = mf_water_psys_bar(in->water_k);
                    if (p_vap > p_sys) p_sys = p_vap;
                }
                if (p_sys > MF_CAP_BAR) p_sys = MF_CAP_BAR;
            }
            e->water_a += MF_WATER_A_GROW * sev_g * clock_drive * grow * (1.0 + rev) * dt;
            {
                double dp = p_sys - 1.0;
                double flow = sqrt(dp > 0.0 ? dp / MF_WATER_P_REF_BAR : 0.0);
                /* T2: e->water_a is the leak AREA (1.0 = stock at onset); the
                 * loss empties K litres over one budget at reference flow,
                 * x severity, x the slowly growing area */
                double loss = MF_WATER_LEAK_K * sev_g
                              * (0.5 + 0.5 * e->water_a)
                              * (flow / MF_WATER_P_REF_FLOW)
                              * clock_drive * grow * dt;
                double v_new = mf_clamp(e->v_w_l - loss, 0.0, MF_WATER_START_L);
                loss = e->v_w_l - v_new;              /* actual escape this substep */
                e->v_w_l = v_new;
                /* E8g item 3: steam only while coolant is escaping AND the
                 * water runs above the car's baseline + 10 K; density from
                 * the escape rate */
                {
                    double wb = mf_water_base(e, in);
                    if (loss > 0.0 && in->water_k > wb + MF_WATER_STEAM_DK
                        && in->water_k >= MF_WATER_STEAM_MIN_K) {
                        double rate = loss / dt;
                        double r = mf_clamp(rate / MF_WATER_SMOKE_LPS, 0.0, 1.0);
                        double s = -mf_clamp(0.1 + 0.5 * r, 0.1, 0.6);
                        if (s < out->smoke_bank1) out->smoke_bank1 = s;
                    }
                }
            }
        }
        /* E8g item 4: debris scales ONLY the radiator airflow terms
         * (rad_air_scale); coolant loss scales all three (rad_scale) */
        {
            double cool_frac = e->v_w_l / MF_WATER_START_L;
            if (e->v_w_l < MF_CAVITATION_L) cool_frac *= MF_CAVITATION_SCALE;
            if (e->fault == MF_FAULT_WATER_LEAK) {
                out->rad_scale = cool_frac;
                out->rad_air_scale = cool_frac;
            } else {
                /* R3e: debris scales the airflow AND a large share of the
                 * base dissipation (see MF_DEBRIS_BASE_KEEP). */
                out->rad_scale = MF_DEBRIS_BASE_KEEP
                                 + (1.0 - MF_DEBRIS_BASE_KEEP) * e->f_rad;
                out->rad_air_scale = e->f_rad;
            }
        }
        if (e->f_rad < MF_RAD_DEBRIS_WARN) out->warn_pit_debris = 1;
        /* R3g: steam/air sensor, heat to the oil/metal, steam while gone */
        if (e->fault == MF_FAULT_WATER_LEAK
            && e->v_w_l < MF_WATER_SENSOR_FRAC * MF_WATER_START_L) {
            double lack = 1.0 - e->v_w_l / (MF_WATER_SENSOR_FRAC * MF_WATER_START_L);
            /* L2 2026-10-05: a low coolant level is its own diagnosis - it does
             * NOT claim a water temperature (owner live 2026-10-05: WATER TEMP
             * at 79 C on a normal engine). Steam, oil heat and the dry gasket
             * creep below are unchanged (R3g). */
            out->warn_coolant_low = 1;
            out->oil_heat_ks += MF_WATER_OIL_HEAT_KPS * lack;
            /* N3 2026-10-06 (H10): steam is only real above the boiling point -
             * the low-coolant path now shares the escape-steam floor
             * (MF_WATER_STEAM_MIN_K = 100 C); it puffed steam at 84 C before. */
            if (in->water_k >= MF_WATER_STEAM_MIN_K
                && out->smoke_bank1 > MF_WATER_LOW_STEAM)
                out->smoke_bank1 = MF_WATER_LOW_STEAM;
            /* T2: with the coolant gone the metal heat ends the gasket; the
             * live game water temperature does not follow the levers */
            e->h_g = mf_clamp(e->h_g - MF_GASKET_DRY_K * clock_drive * lack
                              * grow * growth_scale * dt, 0.0, 1.0);
        }
        {
            double wb = mf_water_base(e, in);
            if (in->water_k >= wb + MF_WATER_WARN_DK
                && in->water_k >= MF_WATER_WARN_MIN_K) out->warn_water_temp = 1;
            if (in->water_k >= wb + MF_WATER_COOL_DK
                && in->water_k >= MF_WATER_COOL_MIN_K) out->warn_cool_engine = 1;
        }
        /* E9b-2 item 7: cooling cascade - sustained water above the warning
         * margin + 18 K creeps the head gasket (~0.5 after 120 s at that
         * temperature); the E9e gasket coupling then starts. Stabilised
         * faults stop growing (conservative: same rule as the fault itself). */
        {
            double wb = mf_water_base(e, in);
            if (in->water_k > wb + MF_WATER_WARN_DK + 18.0) {
                e->h_g = mf_clamp(e->h_g - MF_CASCADE_GASKET_K * grow
                                  * growth_scale * dt, 0.0, 1.0);  /* H2d */
                out->cascade_kind = 2;
            }
        }
        if (in->water_k > MF_GASKET_K) {
            e->h_g = mf_clamp(e->h_g
                     - MF_GASKET_RATE * mf_pow2((in->water_k - MF_GASKET_K) / 6.0)
                       * grow * growth_scale * dt,     /* H2d: common scale */
                     0.0, 1.0);
        }
        if (!fair && e->h_g <= 0.0) {               /* H2a: floor first */
            e->stopped = 1;
            out->smoke_bank1 = -0.6;                      /* white smoke */
            snprintf(out->reason, sizeof out->reason, "head gasket");
        }
        break;
    }

    /* 4.4 piston rings ------------------------------------------------ */
    case MF_FAULT_BLOWBY: {
        double w_pis_ref = mf_pow2((c->oil_opt_k + MF_PIS_RISE_K * MF_LOAD_REF * MF_REV_REF
                                    - MF_PIS_LIM_K) / 15.0);
        double phys = ((1.1 - e->ring_s) / 1.0) * (load / MF_LOAD_REF)
                      * (mf_pow2((e->t_pis_k - MF_PIS_LIM_K) / 15.0) / w_pis_ref);
        e->ring_s = mf_clamp(e->ring_s - MF_RING_K * sev_g * clock_drive * phys * grow * dt,
                             0.0, 1.0);
        e->v_oil_l = mf_clamp(e->v_oil_l
                     - MF_RING_BLOWBY_LPS * (1.0 - e->ring_s) * load * dt, 0.0, MF_OIL_START_L);
        if (!fair && e->ring_s < MF_RING_STOP) {    /* H2a: floor first */
            e->stopped = 1;
            out->smoke_blue = MF_RING_BLUE;
            snprintf(out->reason, sizeof out->reason, "blow-by");
        }
        out->smoke_blue = mf_clamp(1.0 - e->ring_s, 0.0, 1.0);
        break;
    }

    /* 4.5 exhaust crack ----------------------------------------------- */
    case MF_FAULT_EXHAUST: {
        /* thermal fatigue on reversal (Paris law). A reversal is confirmed
         * once the gas temperature has moved MF_PARIS_MIN_SWING_K off the
         * running extreme; the completed half-cycle amplitude is used. */
        if (e->t_exh_k > e->t_exh_peak_k)   e->t_exh_peak_k = e->t_exh_k;
        if (e->t_exh_k < e->t_exh_trough_k) e->t_exh_trough_k = e->t_exh_k;
        if (e->t_exh_dir >= 0.0
            && e->t_exh_peak_k - e->t_exh_k > MF_PARIS_MIN_SWING_K) {
            double dsw = e->t_exh_peak_k - e->t_exh_trough_k;  /* full amplitude */
            if (dsw > MF_PARIS_MIN_SWING_K && e->crack_a < 1.0) {
                double g = MF_PARIS_C * pow(dsw / MF_PARIS_DT_REF_K * sqrt(e->crack_a), MF_PARIS_M);
                e->crack_a += g * sev_g * grow * growth_scale;   /* H2d */
            }
            e->t_exh_dir = -1.0;
            e->t_exh_trough_k = e->t_exh_k;   /* measure the new down-swing */
        } else if (e->t_exh_dir <= 0.0
                   && e->t_exh_k - e->t_exh_trough_k > MF_PARIS_MIN_SWING_K) {
            double dsw = e->t_exh_peak_k - e->t_exh_trough_k;  /* full amplitude */
            if (dsw > MF_PARIS_MIN_SWING_K && e->crack_a < 1.0) {
                double g = MF_PARIS_C * pow(dsw / MF_PARIS_DT_REF_K * sqrt(e->crack_a), MF_PARIS_M);
                e->crack_a += g * sev_g * grow * growth_scale;   /* H2d */
            }
            e->t_exh_dir = 1.0;
            e->t_exh_peak_k = e->t_exh_k;     /* measure the new up-swing */
        }
        /* vibration growth */
        if (e->crack_a < 1.0)
            e->crack_a += dt * MF_CRACK_VIB_K * rev * rev * e->crack_a
                          * grow * growth_scale;                 /* H2d */
        if (e->crack_a > 1.0) e->crack_a = 1.0;
        /* hot gas onto the rear (R3m: x MF_REAR_HEAT_K so every severity ends
         * inside its budget at race pace - R3i rule) */
        if (e->crack_a > 0.05 && e->t_exh_k > ambient_k) {
            e->rear_heat += dt * MF_REAR_HEAT_K * e->crack_a
                            * (e->t_exh_k - ambient_k) / MF_EXH_RISE_K;
            if (e->rear_heat >= MF_REAR_HEAT_MAX && !e->rear_broken) {
                e->rear_broken = 1;
                out->break_rear_suspension = 1;
            }
        }
        out->pop_chance_s = e->crack_a < 1.0 ? MF_CRACK_POP_K * e->crack_a
                                             : MF_CRACK_POP_K * 2.0;
        break;
    }

    /* 4.6 fuel system -------------------------------------------------- */
    case MF_FAULT_FUEL_PRESSURE:
    case MF_FAULT_FUEL_LEAK: {
        double demand, fp, shortfall, flow_ref;
        if (e->fault == MF_FAULT_FUEL_PRESSURE) {
            e->fuel_cap -= MF_FUEL_CAP_K * sev_g * clock_drive * grow * dt;
            if (e->fuel_cap < MF_FUEL_CAP_FLOOR) e->fuel_cap = MF_FUEL_CAP_FLOOR;
        }
        /* E9e item 4: the demand reference is the car's own consumption at
         * full throttle and the limit (native formula, FuelConsumption x
         * FuelConsumptionMultiplier); when the car value is missing (def flag
         * 2048) the session-peak normalisation stays as the fallback. */
        flow_ref = c->fuel_cons > 0.0
                 ? c->fuel_cons * c->rev_limit_rads * c->fuel_mult : 0.0;
        /* R3n (ADDENDUM 11): the PLR Fuel Consumption Multiplier (owner's 2x
         * in practice) is not in the car's reference, so the native reference
         * alone reads demand > 1 at full throttle and the leak/pressure
         * branch cut power. Normalise with the session peak (>= the car
         * reference) so demand <= 1 at full throttle by construction. */
        if (e->fuel_flow_peak_lps > flow_ref) flow_ref = e->fuel_flow_peak_lps;
        demand = flow_ref > 1e-9 ? e->fuel_flow_lps / flow_ref
               : (e->fuel_flow_peak_lps > 1e-9
                  ? e->fuel_flow_lps / e->fuel_flow_peak_lps : 0.0);
        shortfall = 0.0;
        if (e->fault == MF_FAULT_FUEL_PRESSURE
            && demand > e->fuel_cap && demand > 1e-9) {
            shortfall = mf_clamp(1.0 - e->fuel_cap / demand, 0.0, 1.0);
            /* R3h: symptom curve on the pressure fraction, scaled by load */
            {
                double p = e->fuel_cap;
                double lf = MF_FP_LOAD_MIN
                            + (1.0 - MF_FP_LOAD_MIN) * mf_clamp(load, 0.0, 1.0);
                if (p < MF_FP_STUMBLE) {
                    double lack = mf_clamp((MF_FP_STUMBLE - p)
                                           / (MF_FP_STUMBLE - MF_FP_HEAVY), 0.0, 1.0);
                    e->fp_stumble_acc_s += dt;
                    if (e->fp_stumble_acc_s >= 1.0) {   /* one roll per second */
                        e->fp_stumble_acc_s -= 1.0;
                        if (mf_rng_d(&e->rng) < lack * lf) {
                            e->fp_stumble_s = 1.5;
                            e->fp_stumble_d = 0.3 + 0.7 * lack;
                        }
                    }
                    if (p < MF_FP_CUT_FULL) {           /* clear top-end cut */
                        double cut = mf_clamp((MF_FP_CUT_FULL - p)
                                              / (MF_FP_CUT_FULL - MF_FP_HEAVY), 0.0, 1.0);
                        out->loss_top = cut * lf;
                    }
                    if (p < MF_FP_HEAVY) {              /* native lean-cut lever */
                        double fr = mf_clamp((p - MF_FUEL_CAP_FLOOR)
                                             / (MF_FP_HEAVY - MF_FUEL_CAP_FLOOR), 0.0, 1.0);
                        out->vfuel_press = MF_FUEL_CUT_MIN + MF_FUEL_CUT_SPAN * fr;
                    }
                }
            }
            /* R3i: no silent permanent hold on the floor - fuel starvation
             * ends the engine (fairness floor respected). */
            if (e->fuel_cap <= MF_FUEL_CAP_FLOOR + 1e-9)
                e->fp_floor_s += dt;
            if (e->fp_floor_s >= MF_FP_STARVE_S && !fair) {
                e->stopped = 1;
                snprintf(out->reason, sizeof out->reason, "fuel starvation");
            }
        } else {
            e->fp_stumble_acc_s = 0.0;
        }
        e->lean_shortfall = shortfall;
        if (shortfall > 0.0) {
            /* lean running wears the rings at half rate (4.6) */
            double w_pis_refL = mf_pow2((c->oil_opt_k + MF_PIS_RISE_K * MF_LOAD_REF * MF_REV_REF
                                         - MF_PIS_LIM_K) / 15.0);
            double phys = (1.1 - e->ring_s) * (load / MF_LOAD_REF)
                          * (mf_pow2((e->t_pis_k - MF_PIS_LIM_K) / 15.0) / w_pis_refL);
            e->ring_s = mf_clamp(e->ring_s
                         - 0.5 * MF_RING_K * shortfall * clock_drive * phys * grow * dt,
                         0.0, 1.0);
        }
        if (e->fault == MF_FAULT_FUEL_LEAK) {
            fp = 0.4 + 0.6 * load;
            e->fuel_lost_l += MF_FUEL_LEAK_LPS * sev_g * sqrt(fp) * dt;  /* E4 removes it */
            e->fire_acc_s += dt;
            if (e->fire_acc_s >= 1.0) {                  /* one fire roll per second */
                double hazard = MF_FIRE_P * sev_g
                                * ((e->t_exh_k - MF_FIRE_T_IGN_K) / 100.0);
                int fair_ok = e->t_s >= e->onset_t_s + MF_FAIR_FRAC * e->ttf_onset_s;
                e->fire_acc_s -= 1.0;
                if (fair_ok && hazard > 0.0 && mf_rng_d(&e->rng) < hazard) {
                    e->blown = 1;
                    snprintf(out->reason, sizeof out->reason, "fuel fire");
                }
            }
        }
        if (e->h_b < MF_BEARING_ROUGH) {
            double cut = 0.6;
            if (out->vfuel_press < 0.0 || cut < out->vfuel_press) out->vfuel_press = cut;
        }
        break;
    }

    default:
        break;
    }

    /* E9b item 1 (fairness floor): while inside onset + MF_FAIR_FRAC x
     * time-to-fail-at-onset, the failing components are pinned at
     * MF_FAIR_HOLD AFTER the damage of this substep, so a model terminal
     * cannot fire early and the value never appears to rise between samples
     * (the terminal checks above already saw the pinned value). The E9a
     * torque-zero death sits outside this path - driver error, exempt. */
    if (e->t_s < e->onset_t_s + MF_FAIR_FRAC * e->ttf_onset_s) {
        if (e->h_vt < MF_FAIR_HOLD) e->h_vt = MF_FAIR_HOLD;
        if (e->h_b < MF_FAIR_HOLD) e->h_b = MF_FAIR_HOLD;
        if (e->h_g < MF_FAIR_HOLD) e->h_g = MF_FAIR_HOLD;
        if (e->h_rod < MF_FAIR_HOLD) {
            e->h_rod = MF_FAIR_HOLD;
            e->h_rod_dmg = 1.0 - MF_FAIR_HOLD;
        }
        /* the rings stop at MF_RING_STOP, not 0: hold just above it */
        if (e->ring_s < MF_RING_STOP + MF_FAIR_HOLD)
            e->ring_s = MF_RING_STOP + MF_FAIR_HOLD;
    }

outputs:
    /* 2026-10-06 owner: water warnings for ANY overheating, not only during a
     * cooling fault (the game itself has no water warning). Outside a cooling
     * fault the absolute physical markers alone decide; the cooling-fault
     * case below keeps its car-relative margin as well. Player only; none
     * with Failure Rate 'none' (no consequence exists then). */
    if (!(e->onset && (e->fault == MF_FAULT_WATER_LEAK
                       || e->fault == MF_FAULT_RAD_DEBRIS))
        && !in->ai_mode && !in->life_infinite && in->on_track
        && in->engine_running && !e->culminated && !e->blown && !e->stopped) {
        if (in->water_k >= MF_WATER_SETUP_WARN_K) out->warn_water_temp = 1;
        if (in->water_k >= MF_WATER_SETUP_COOL_K) out->warn_cool_engine = 1;
    }

    /* E9b item 3: harmless niggles - fun, never a failure. Only while no
     * onset is active, on track, engine running; mean MF_NIGGLE_PER_H per
     * hour, at most MF_NIGGLE_MAX per session. They never touch life, health
     * or anything that can cascade. */
    if (e->niggle_left_s > 0.0) {
        if (!in->niggles_enable) {
            /* E9-H2e/D5: disabling niggles clears an ACTIVE one at once
             * (the DLL then ends the misfire/bank/sensor output) */
            e->niggle_left_s = 0.0;
            e->niggle_kind = 0;
        } else {
            e->niggle_left_s -= dt;
            if (e->niggle_left_s <= 0.0) {
                e->niggle_left_s = 0.0;
                e->niggle_kind = 0;
            }
        }
    }
    if (!e->onset && !e->culminated && !e->blown && !e->stopped
        && in->niggles_enable && !in->life_infinite  /* 2026-10-06 vanilla+ */
        && in->on_track && in->engine_running
        && e->niggle_kind == 0 && e->niggle_count < MF_NIGGLE_MAX) {
        e->niggle_acc_s += dt;
        if (e->niggle_acc_s >= 1.0) {          /* one hazard roll per second */
            double p = mf_niggle_rate(e);
            e->niggle_acc_s -= 1.0;
            if (mf_rng_d(&e->rng) < p) {
                double u = mf_rng_d(&e->rng);
                e->niggle_count++;
                if (u < 1.0 / 3.0) {
                    e->niggle_kind = MF_NIGGLE_MISFIRE;
                    e->niggle_left_s = mf_rand_range(&e->rng, MF_NIGGLE_MISFIRE_MIN_S,
                                                     MF_NIGGLE_MISFIRE_MAX_S);
                } else if (u < 2.0 / 3.0) {
                    e->niggle_kind = MF_NIGGLE_SENSOR;
                    e->niggle_left_s = mf_rand_range(&e->rng, MF_NIGGLE_SENSOR_MIN_S,
                                                     MF_NIGGLE_SENSOR_MAX_S);
                } else {
                    e->niggle_kind = MF_NIGGLE_PUFF;
                    e->niggle_left_s = MF_NIGGLE_PUFF_S;
                }
            }
        }
    } else if (!in->niggles_enable || e->onset) {
        e->niggle_acc_s = 0.0;
    }
    out->niggle_kind = e->niggle_kind;
    out->niggle_left_s = e->niggle_left_s;

    /* 4.8 power product (E9e): the E1-E8 flat bearing and ring factors are
     * now the located losses of the torque-curve power shape below; only the
     * fallen-off exhaust (x0.9) stays a flat factor. */
    {
        double p = 1.0;
        if (e->crack_a >= 1.0) p *= 0.9;
        out->power_mult = p < MF_POWER_MIN ? MF_POWER_MIN : p;
        /* E9e power shape: bearings at all rpm, rings with throttle (the
         * valve return and fuel pressure cases set loss_top while active) */
        out->loss_all = 0.3 * (1.0 - e->h_b);
        out->loss_load = 0.4 * (1.0 - e->ring_s);
    }
    /* E9d item 3: rhythmic misfire depth from the worst worn component
     * (valvetrain < 0.6, bearings < 0.5, rings < 0.5), inertia-scaled; the
     * fuelcut hook pulses the native output multiplier with it. No effects
     * once the engine is dead. */
    if (!e->blown && !e->stopped && !e->culminated) {
        double base = 0.0;
        if (e->h_vt < 0.6) {
            double d = MF_MISFIRE_MAX * (0.6 - e->h_vt) / 0.6;
            if (d > base) base = d;
        }
        if (e->h_b < 0.5) {
            double d = MF_MISFIRE_MAX * (0.5 - e->h_b) / 0.5;
            if (d > base) base = d;
        }
        if (e->ring_s < 0.5) {
            double d = MF_MISFIRE_MAX * (0.5 - e->ring_s) / 0.5;
            if (d > base) base = d;
        }
        /* R3h: an active fuel-pressure stumble rides the same pattern; it is
         * a developing real fault, so it never uses the discrete cuts. */
        {
            double wear_base = base;
            if (e->fp_stumble_s > 0.0) {
                e->fp_stumble_s -= dt;
                if (e->fp_stumble_s < 0.0) e->fp_stumble_s = 0.0;
                if (base < e->fp_stumble_d) base = e->fp_stumble_d;
            }
            out->misfire_depth = mf_misfire_depth_scaled(c, base);
            out->misfire_discrete = wear_base > 0.0 ? 1 : 0;
        }
        /* E9b item 3: a niggle MISFIRE is the E9d pattern at depth 0.3 */
        if (e->niggle_kind == MF_NIGGLE_MISFIRE && e->niggle_left_s > 0.0) {
            if (out->misfire_depth < MF_NIGGLE_MISFIRE_D)
                out->misfire_depth = MF_NIGGLE_MISFIRE_D;
            out->misfire_discrete = 0;      /* H2e: never discrete cuts */
        }
        /* E9-H2b: an emergency onset is felt IMMEDIATELY (misfire + a small
         * power cut), before any radio/HUD line can be scheduled. This is a
         * real developing fault, so the discrete cuts are allowed. */
        if (e->emergency_onset) {
            if (out->misfire_depth < MF_EMERGENCY_MISFIRE_D)
                out->misfire_depth = MF_EMERGENCY_MISFIRE_D;
            if (out->power_mult > MF_EMERGENCY_POWER)
                out->power_mult = MF_EMERGENCY_POWER;
            out->misfire_discrete = 1;
        }
    }
    /* E9d item 4: extra burn - blow-by leaks charge and lean running burns
     * more fuel; the DLL moves it onto FUEL_NOW (same path as the leak) */
    if (in->engine_running && !e->culminated && !e->blown && !e->stopped) {
        out->fuel_extra_lps = (MF_BLOWBY_BURN_K * (1.0 - e->ring_s)
                               + MF_LEAN_BURN_K * e->lean_shortfall)
                              * e->fuel_flow_lps;
    }
    out->blow_up = e->blown;
    out->engine_stop = e->stopped;
    out->break_rear_suspension = out->break_rear_suspension || e->rear_broken;
    if (e->h_vt < MF_VT_SHIFT_EARLY) out->warn_shift_earlier = 1;
    if (e->blown || e->stopped) {
        out->rev_ceiling = -1.0;
        out->vfuel_press = -1.0;
        out->vfuel_valve = -1.0;
        out->loss_top = 0.0;
        out->loss_load = 0.0;
        out->loss_all = 0.0;
        out->misfire_depth = 0.0;
        out->fuel_extra_lps = 0.0;
        out->oil_gauge_scale = 1.0;
    }
}

/* ------------------------------------------------------------------ */
/* Public step: onset once per call, then fixed-step physics.          */
/* ------------------------------------------------------------------ */

/* E9-H2c (R13): reject nonfinite inputs before derive/step/write. Every raw
 * value falls back to a documented safe default; a nonfinite clock is treated
 * as unknown (huge) so it can never onset, culminate or terminate. */
static inline void mf_inputs_sanitize(MfInputs *in)
{
    if (!isfinite(in->dt_s)) in->dt_s = 0.0;
    if (!isfinite(in->life_s)) in->life_s = 1e12;
    if (!isfinite(in->life_drain_sps)) in->life_drain_sps = 0.0;
    if (!isfinite(in->rpm_rads)) in->rpm_rads = 0.0;
    if (!isfinite(in->oil_k)) in->oil_k = MF_AMBIENT_FALLBACK_K;
    if (!isfinite(in->water_k)) in->water_k = MF_AMBIENT_FALLBACK_K;
    if (!isfinite(in->load)) in->load = 0.0;
    if (!isfinite(in->speed_mps)) in->speed_mps = 0.0;
    if (!isfinite(in->impulse) || in->impulse < 0.0) in->impulse = 0.0;
    if (!isfinite(in->fuel_l)) in->fuel_l = 0.0;
    if (!isfinite(in->g_long)) in->g_long = 0.0;
    if (!isfinite(in->g_lat)) in->g_lat = 0.0;
    if (!isfinite(in->ambient_c)) in->ambient_c = MF_AMBIENT_FALLBACK_K - 273.15;
    if (!isfinite(in->stress)) in->stress = 0.0;
}

static inline void mf_model_step(MfEngine *e, const MfCar *c, const MfInputs *in,
                                 MfOut *out)
{
    MfInputs ins = *in;                    /* H2c: sanitize a copy, callers stay const */
    mf_out_default(out);
    mf_inputs_sanitize(&ins);
    in = &ins;
    if (!(in->dt_s > 0.0) || in->dt_s > MF_DT_MAX_S) {
        e->acc_s = 0.0;                    /* E9o: clock jump - skip, re-base */
        return;
    }
    /* N5 2026-10-06 (owner-approved): the post-REPAIR grace counts down in
     * game time while on track; no AUTOMATIC onset path may fire while > 0
     * (the gates below sit in the model's own onset paths only - a manual
     * mf_fault_arm from the panel still works). */
    if (e->repair_grace_s > 0.0 && in->on_track) {
        e->repair_grace_s -= in->dt_s;
        if (e->repair_grace_s < 0.0) e->repair_grace_s = 0.0;
    }
    mf_onset_step(e, c, in);
    /* P2 (ADDENDUM 2) + Q3 (ADDENDUM 3, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt):
     * hazard onset. x = consumed / budget with the budget = the first valid
     * life sample (the engine's life at model start: player ENG_LIFE, AI
     * timer/divisor). The hazard fires at most once; with onset_model 0 its
     * result is SHADOW ONLY (P2, logs only - byte-identical to 911b619d).
     * With onset_model 1 (default) it applies the onset: fault from the
     * accumulated damage history, severity/stabilise from the main stream as
     * today; the development budget ttf_onset_s = max(MF_LEAD_MIN_S,
     * min(life_s/drain_ema, lead_s)) keeps the fairness floor and warnings
     * their window. The countdown path stays as backstop (fires if the hazard
     * did not). */
    if (e->budget_s <= 0.0 && in->life_s > 1.0 && in->life_s < 1e12
        && !in->life_infinite)
        e->budget_s = in->life_s;
    if (!e->shadow_fired && e->budget_s > 0.0 && e->dmg_total > 0.0
        && e->repair_grace_s <= 0.0) {   /* N5: hazard deferred, not consumed */
        double x_now = e->dmg_total / e->budget_s;
        double u = mf_rng_d(&e->hz_rng);
        if (mf_hazard_step(e->hz_x, x_now, e->hz_beta, e->hz_eta, u)) {
            int f = mf_fault_history(e);
            e->shadow_fired = 1;
            e->shadow_fault = f;
            e->shadow_x = x_now;
            e->shadow_fire_t = e->t_s;
            if (in->onset_model && !e->onset && !e->culminated && !e->blown
                && !e->stopped && in->life_s > 0.0 && !in->life_infinite) {
                int stab = 0;
                int sev = mf_roll_severity(e, &stab);
                double delay = -1.0;
                double ttf;
                if (stab)
                    delay = mf_rand_range(&e->rng, MF_STABILISE_MIN_S,
                                          MF_STABILISE_MIN_S + MF_STABILISE_SPAN_S);
                mf_onset_apply(e, in, f, sev, delay);
                ttf = in->life_s / (e->drain_ema_sps > MF_DRAIN_MIN_SPS
                                    ? e->drain_ema_sps : MF_DRAIN_MIN_SPS);
                if (ttf > e->lead_s) ttf = e->lead_s;
                if (ttf < MF_LEAD_MIN_S) ttf = MF_LEAD_MIN_S;
                e->ttf_onset_s = ttf;
                /* Development budget (Q3b): the hazard onset anchors the
                 * fault's growth pace on ttf_onset_s, exactly as
                 * mf_arm_budget does for the virtual-budget arms
                 * (clock_drive = 1/ttf). Without this a hazard that fires
                 * early on a long AI budget would never develop to its
                 * terminal inside the race. The remaining game clock keeps
                 * its own meaning; the fairness floor uses ttf. */
                if (e->drain_onset_sps > MF_DRAIN_MIN_SPS)
                    e->life_onset_s = ttf * e->drain_onset_sps;
                e->onset_hazard = 1;
            }
        }
        e->hz_x = x_now;
    }
    /* Q7a (ADDENDUM 7, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): with the AI
     * countdown (ai_mode 1, onset_model 0) a fault budget may never outlast
     * the remaining ai_life / ai_rate - cap ttf at it (floor 0) so the
     * development lands no later than the vanilla WHEN. The hard terminal
     * below is the backstop whatever the fault stage. Player runs and AI
     * hazard runs keep their budget rules. */
    if (in->ai_mode && !in->onset_model && !in->life_infinite
        && !in->ai_nursed
        && e->onset && !e->culminated && !e->blown && !e->stopped) {
        double drain = e->drain_ema_sps > MF_DRAIN_MIN_SPS
                     ? e->drain_ema_sps : MF_DRAIN_MIN_SPS;
        double rem = in->life_s / drain;
        if (rem < 0.0) rem = 0.0;
        if (e->ttf_onset_s > rem) {
            e->ttf_onset_s = rem;
            e->life_onset_s = rem * e->drain_onset_sps;  /* pace = 1 / rem */
        }
    }
    mf_shift_step(e, c, in);               /* E9o: 0.2 s downshift window, game time */
    mf_impact_step(e, c, in, out);         /* E9h: contact faults from wall hits */
    e->acc_s += in->dt_s;
    if (e->acc_s > MF_DT_MAX_S) e->acc_s = MF_DT_MAX_S;   /* a stalled pump is a jump */
    while (e->acc_s >= MF_MODEL_DT) {
        mf_substep(e, c, in, out);
        e->acc_s -= MF_MODEL_DT;
    }
    /* Q7a (ADDENDUM 7, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): AI countdown
     * HARD terminal - the AI clock reaching zero IS the vanilla WHEN. Whatever
     * the fault stage (developing, culminated, stabilised, no onset yet) the
     * model outputs the terminal and the caller writes the game's 0.001. Only
     * the AI countdown path (ai_mode 1, onset_model 0); the player and AI
     * hazard runs are untouched. */
    /* B6 (ADDENDUM 14): a NURSED AI slot is not hard-killed at life zero -
     * the physics decides (the reaction changes the real rpm/load, so a
     * nurse can hold the fault and reach the flag; an ignored fault keeps the
     * exact vanilla WHEN below). */
    if (in->ai_mode && !in->onset_model && !in->life_infinite
        && !in->ai_nursed
        && in->life_s <= 0.0 && !e->blown && !e->stopped) {
        e->stopped = 1;
        snprintf(out->reason, sizeof out->reason, "engine life expired");
    }
    /* E9b item 2: with no onset, the always-on layers (over-rev jumps, Miner
     * fatigue, engine braking, shift shocks) may wear but never drive h_vt,
     * h_b or h_rod below MF_ALWAYS_ON_FLOOR (E9a review: covers the over-rev
     * jump damage to the rods). */
    if (!e->onset && !e->culminated && !e->blown && !e->stopped) {
        if (e->h_vt < MF_ALWAYS_ON_FLOOR) e->h_vt = MF_ALWAYS_ON_FLOOR;
        if (e->h_b  < MF_ALWAYS_ON_FLOOR) e->h_b  = MF_ALWAYS_ON_FLOOR;
        if (e->h_rod < MF_ALWAYS_ON_FLOOR) {
            e->h_rod = MF_ALWAYS_ON_FLOOR;
            e->h_rod_dmg = 1.0 - MF_ALWAYS_ON_FLOOR;
        }
    }
    e->overrev_prev_above = (in->rpm_rads > c->tq_zero_rads) ? 1 : 0;
    out->blow_up = e->blown;
    out->engine_stop = e->stopped;
    out->break_rear_suspension = out->break_rear_suspension || e->rear_broken;
}

#endif /* MF_ENGINE_MODEL_H */
