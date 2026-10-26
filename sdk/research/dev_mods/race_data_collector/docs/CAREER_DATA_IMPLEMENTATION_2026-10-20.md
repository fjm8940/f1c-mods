# Career data implementation (schema 2.1, producer 0.3.0) - 2026-10-20

Spec: `docs/SPEC_CAREER_DATA_2026-10-20.md`. Field-level contract: `docs/SCHEMA_2_1.md`. Owner checks: `docs/LIVE_TESTS_CAREER_DATA_2026-10-20.md`.
Stage: implement and offline-verify (dev only). No deploy, no copy to `production\`, no game run, no live gate. Native validation pending.

## Changed files (all under `development\mods\race_data_collector\`)
- `src\collector.hpp`, `src\collector.cpp`: schema 2.1 fields, derived sectors, lap-end gaps, contact pairing, retirement detail, pit tyres, fuel per lap, endReason, unavailable reasons.
- `src\metadata.hpp`, `src\metadata.cpp`: `season_year_from_gdb`, track folder, RCD driver name.
- `src\plugin.cpp`: read-only vehicle-block reads (tread, pace scalar, aggression, accel, lap distance, GDB planned stops), version 0.3.0 log lines.
- `tests\core_tests.cpp`: schema 2.1 assertions (groups 1-12) plus the 2.0 section-compatibility check (41 sections).
- `build.ps1`: optional `-OutDir` (default unchanged). The existing-directory `New-Item` now tolerates an existing directory.
- `tools\season_narrative_bridge.py`: accepts schema 2.1 as well as 2.0 (2.0 rules unchanged).
- New: `docs\SCHEMA_2_1.md`, `docs\CAREER_DATA_IMPLEMENTATION_2026-10-20.md` (this file), `docs\LIVE_TESTS_CAREER_DATA_2026-10-20.md`.
- Updated docs: `README.md` (index entry), `STATUS.md` (entry), `docs\DATA_COVERAGE.md` (schema pointer).

## Before-copies (`<file>.BEFORE_20261020_careerdata`, created before the first edit, not deleted)
src\collector.cpp, src\collector.hpp, src\metadata.cpp, src\metadata.hpp, src\plugin.cpp, tests\core_tests.cpp, build.ps1, tools\season_narrative_bridge.py, README.md, STATUS.md, docs\DATA_COVERAGE.md.

## Build
- Output: `%TEMP%\rdc_careerdata_20261020` (not in the source tree).
- `race_data_collector.dll` SHA-256 `57ACADFED490862673F9356CC7A8E91DC44A988CB855F3926A6397A687049D86`.
- Path: `%USERPROFILE%\AppData\Local\Temp\rdc_careerdata_20261020\race_data_collector.dll` (scratch; not deployed).
- Toolchain present (`C:\msys64\mingw32`). Compile flags as before (`-Wall -Wextra -Wpedantic`).
- `build.ps1` exited 1 at the Python consumer step (Python not installed; the Store alias only). The DLL and C++ tests ran before it.
- Stale scratch folder `fixtures` from the first aborted build was renamed to `fixtures_stale_before_run2` (not deleted); it held a journal that made the storage test fail.

## Tests (offline)
| Suite | Result |
|---|---|
| `core_tests.exe` (C++ assertions, one binary) | PASS (includes all new schema 2.1 assertions and the 41-section 2.0 compatibility check) |
| `dll_guard_test.exe` load/guard | PASS |
| `dll_guard_test.exe ... compatible-layout` | PASS |
| `tests\verify_outputs.py` (consumer, Python) | NOT RUN: Python not installed |

Test counts: 3 of 4 suites run and pass; 1 not run (Python). No game process was running at check time.

## Field status (priority groups from the spec)
| # | Group | Status |
|---|---|---|
| 1 | Session identity | season year: native (GDB header, FACT); track folder: native; RCD driver name: native; team: existing VEH `team` (FullTeamName key not proven, not invented); car number, session type, laps, player flag: existing 2.0 |
| 2 | Sector splits | derived (I1, I2-I1, lap-I2); bests derived. Semantics FACT; read offsets SDK-observed (caveat) |
| 3 | Position and gaps | lap-end gaps in seconds derived; gap metres UNAVAILABLE |
| 4 | Tyres | index per lap, stint, tread per lap and per pit visit: CONFIRMED; compound name UNAVAILABLE |
| 5 | Pit stops | every visit kept (existing); tyre before/after, reason (planned match only), pit visit count, tyre wear at exit |
| 6 | Retirements | reason code raw, last damage, damage history, contacts in 2 laps, accel peak (uncalibrated); cause UNAVAILABLE |
| 7 | Contacts | nearest-car pairing (25 m radius, broker world X/Z), lap, lap distance, uncalibrated accel peak |
| 8 | Penalties | count and lap native; type and served UNAVAILABLE |
| 9 | Qualifying and grid | planned grid (existing), best lap (existing), best sectors derived; qualifying sectors and session UNAVAILABLE |
| 10 | Conditions | wetness and temperatures per lap; rain intensity UNAVAILABLE (existing meaning kept) |
| 11 | AI context | per-car pace scalar and aggression: native; AI strength and realism UNAVAILABLE |
| 12 | Interrupted races | `endReason` added; `final:false` snapshots unchanged; crash not provable |

Coordinator addendum (telemetry port), status: accel (1) DONE; tyre wear (2) DONE; component life (3) DONE, wear gate 0x32C8 UNAVAILABLE (it is the active flag); lap distance (4) PARTIAL (recorded at contacts, gap metres UNAVAILABLE); compound name from .tbc (5) UNAVAILABLE (not ported); fuel per lap (6) DONE from native fuel level, rate 0x3990 UNAVAILABLE; planned stops from GDB (7) DONE; wing flags (8) UNAVAILABLE (no per-lap intact proof); AI difficulty 0x7D01A0 (9) UNAVAILABLE. Not used: veh+0x4574, veh+0x4538, telemetry `cs`. Remote CO-OP copies: flagged, not excluded (see decisions).

## Proof and offsets used (new reads only)
- veh+0x363C/0x3734/0x382C/0x3924 tread: CONFIRMED ai-tyre-tread-wheel-b0-proven-20261009; order FACT vehicle-wheel-block-order-fl-fr-rl-rr-20261013.
- veh+0x44F4 pace scalar: FACT 0x44F4-pace-scalar.
- veh+0x44F8 aggression: CONFIRMED S1-passingnorm-trackaggression-latched.
- veh+0x354C/0x3550/0x3554 accel: FACT ai-accel-inputs-mass-live-2026-09-30.
- veh+0x4044 lap distance: FACT f1c-lapdist-4044-not-4048.
- veh+0x4588 and +0x4590 stride 16 planned stops: FACT f1c-gdb-pitstopstrategies-parsed-into-vehicle-struct.
- Sector semantics: FACT web-telemetry-sectors-cumulative-s2-20261012. Fuel level: existing native field; 0x3984 corroborated as fuel by FACT ai-accel-inputs-mass-live-2026-09-30 (not separately read).
- Season year: FACT replay-season-year-gdb-2026-10-13 (partial: which season file loads at runtime is not proven, so this is the GDB header value of the loaded track).
No new offset was proven by this work, so no entry was appended to `findings.jsonl`.

## Facts vs inference
- Fact: all reads above are read-only; no write path added.
- Inference (flagged in output): contact pairing distance units (metres assumed); cumulative crossing meaning for lap gaps (from one capture); sector offset semantics for the collector's own read addresses.
- Unverified: all live behaviour. Native validation pending (see the live test checklist).

## Decisions for the reviewer
1. Schema 2.1 is what the spec asked for, but the career reader `Lightloader\...\career\Core\F1C\RdcRaceDataReader.cs:106` (and `RdcCaptureImporter`) still requires `f1c.race-data/2.0`. A 2.1 file will be REJECTED by the live career importer until that reader accepts 2.1. Outside this directory, not edited.
2. Sector values are derived and marked. The read offsets for I1/I2 come from the SDK observation header, not from the FACT offset table; the reviewer should accept that or require a live cross-check.
3. Contact pairing radius 25 m (`contact_pair_radius`), nearest non-SC car, own-car hits only. Attribution is a proxy, never blame.
4. Impact acceleration peaks are uncalibrated and must not be used as injury measures without calibration.
5. `endReason` has no `crash` value: a crash cannot be proven natively. A checkpoint (`in_progress`) without a closing export is the evidence.
6. Remote CO-OP copies: NOT excluded. No proven field identifies them natively, so the MP mode is flagged in `capture.issues`. The owner or reviewer must decide the career exclusion rule.
7. Gap metres are not produced. Lap-end gaps are in seconds.
8. `pitReason` only reports a planned-lap match (from the SDK planned-stop offset, which is not in the FACT table). Scheduled, damage and penalty reasons are unavailable.
9. `pitStops` keeps its 2.0 meaning (serviced visits). `pitVisits` is the new all-visit count.
10. Value changes for 2.0 readers are listed in `docs/SCHEMA_2_1.md` section "Value changes".
11. Python consumer test (`verify_outputs.py`) has not run. It must be run on a machine with Python before any promotion.
