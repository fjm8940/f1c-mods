# Spec: collector data for the Lightloader career story - 2026-10-20

Author: Matias (spec: Claude; implementation: Claude Haiku; review: Claude). Stage: specify -> implement (dev only, no release,
no deploy, no production copy). Owner request 2026-10-20: "task haiku to do collector work so that we gather all bits of data our
career mode needs". Career decision D38: the career story lives on what happens in the game; this collector is the harvester.
Evidence base: `development\lightloader\v2.0.0-hub-dev.1\verification\career_game_ux_2026-10-20\RESEARCH_RDC_SIGNALS_2026-10-20.md`
(field inventory, 55 signals) and `RESEARCH_CRASH_DATA_2026-10-20.md`.

## Rules
- Follow `development\mods\MOD_STANDARD.md`, this mod's README/STATUS/docs (incl. `NO_EXE_GATES_2026-09-15.md`,
  `COMPATIBILITY_2026-09-15.md`, `DATA_COVERAGE.md`), and the owner's startup compatibility contract (development\AGENTS.md):
  do not weaken host-protocol verification or code-pattern/layout checks.
- **Read only.** The collector never writes game memory or game files (weather is never touched - D28.3; reading the current
  weather state is allowed).
- Use only memory fields proven in `f1c_ida_decompiled\findings.jsonl` (tier FACT/CONFIRMED) or `docs\38-confirmed-offsets.md`
  (cite the id/line next to each read in a code comment). A field that is not proven stays `null` and gets a `coverage` entry
  `"unavailable: <reason>"` (existing pattern, `collector.cpp:329`). Derived values are marked `"derived": true` or documented.
- Schema change is additive: `f1c.race-data/2.1`; every 2.0 field keeps its name and meaning. Readers of 2.0 must still work.
- Version bump in the source producer string (0.2.6 -> 0.3.0) and in docs. No deploy, no copy into `production\`.
- No game run (no live gates; owner runs live tests). Build and run the mod's own offline tests only.
- Before the first edit of any existing file: copy it to `<file>.BEFORE_20261020_careerdata`.

## Data the career needs (priority order)
1. **Session identity:** season year, track .gdb path/folder name and gdb TrackName/EventName (for the GP history lookup),
   session type, race distance/laps scheduled, player flag per car, driver name exactly as the .rcd first line / .veh Driver,
   team (FullTeamName), car number. (Check what 2.0 already has; fill gaps.)
2. **Sector splits per lap:** raw I1/I2 exist on every lap but `sector1-3` are written as null (`collector.cpp:110`). Write
   sector1 = I1, sector2 = I2 - I1, sector3 = lap - I2 (mark sector3 derived), and `fastestLap.sectorBests`.
3. **Per-lap position and gaps:** position at lap end, gap to leader and to the car ahead (from cumulative times).
4. **Tyres:** compound index per lap/stint and on each pit visit (before/after); tyre condition/wear only if proven.
5. **Pit stops:** every pit-lane visit (not only serviced ones): lap in, stationary time, lane time, tyres fitted, fuel added
   if proven, reason (scheduled/damage/penalty) if derivable.
6. **Retirements (injury evidence, D36):** in the final file per retired car: lap, native reason code, last damage bitfield
   (`nativeCarConditions`) and its change history, and the contacts of that car in the 2 laps before retiring.
7. **Contacts:** pair each `contact_observed` with the other car (nearest car at hit time; mark `"attribution": "nearest-car"`),
   lap, and track position if proven.
8. **Penalties:** type, lap issued, served/not, for each car (counts exist today).
9. **Qualifying/grid:** per-driver best lap + sectors, qualifying position, grid position.
10. **Conditions (read only):** wet/dry state and track/air temperature per lap if proven.
11. **Context:** AI strength and AI Realism (aggression) values in effect, read from the PLR/game state if proven, so the career
    can explain results.
12. **Interrupted races:** keep writing the recovery snapshot with `"final": false` and a clear `"endReason"` (quit, crash,
    unknown) so the career can offer retry/simulate (D9) instead of losing the race.

## Deliverables
- Source changes in `src\`, schema doc update (DATA_COVERAGE.md or a new `SCHEMA_2_1.md`), README/STATUS update, offline tests
  (extend `tests\`) that run the writer on synthetic/recorded inputs and check every new field (types, derived maths: sector3,
  gaps; additive compatibility: a 2.0 reader test still passes).
- Build with `build.ps1` into `%TEMP%\rdc_careerdata_20261020` (not into the source tree), record the DLL SHA-256.
- **Live test checklist** for the owner (`docs\LIVE_TESTS_CAREER_DATA_2026-10-20.md`): a short race with a pit stop, a wall hit
  on an AI car and on the player, a retirement, a penalty; what to look for in the JSON for each field; how to settle the reason
  code 0 vs 6 conflict (`findings.jsonl` id dnf-reasoncode-0-vs-6-crash-meaning-conflict-2026-10-20).
- Record: `docs\CAREER_DATA_IMPLEMENTATION_2026-10-20.md` with changed files, before-copies, hash, test results, which fields are
  live-proven vs unavailable, and decisions for the reviewer.
