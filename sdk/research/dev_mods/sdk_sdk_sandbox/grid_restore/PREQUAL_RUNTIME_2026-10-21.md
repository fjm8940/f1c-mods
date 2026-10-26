# Grid Restore 0.4.0: career pre-qualifying runtime (2026-10-21)

Status: DEV ONLY. Build compiled and offline-tested. No game run, no native or owner validation yet.
Spec: `development\lightloader\v2.0.0-hub-dev.1\SPEC_CAREER_PREQUALIFYING_2026-10-21.md` (runtime part, design items 1 to 4).

## Changed files
- `grid_restore.cpp` (edited; before-state `grid_restore.cpp.BEFORE_20261021_prequal`).
- `career_prequal.hpp` (new, pure policy: header parse, DNQ selection, reduced order; no Windows or game memory).
- Build: `development\lightloader\v2.0.0-hub-dev.1\career_runtime\trials\build_20261021_PREQUAL\lightloader_career.dll`
  SHA-256 `10D85FEEF5FF3F8CCEDA2F058DBB9A9383BD4981B7D78BBA48B25971935511BD`. Previous build kept: `trials\build_20261020_RUNTIME2`.
 . Not copied to any game folder. Game was not running at build time.

## What it does (fact from code, not yet proven live)
1. Reads `#starters<TAB>N` from `career_grid.tsv` (`career_prequal::parse_starters`). Absent = no exclusion (existing behaviour). Malformed, repeated, or outside 2..104 = logged, full field.
2. Runs only inside the existing career apply path (`apply_career_grid`), in session 7 pre-start (phase 0..2, no restart pending), only when `career_weekend.flag` is present, once per session epoch.
3. Field cars planned outside the limit (non-player) leave the race: snapshot of the ordered `VEHICLE_ARRAY`, slot-live (veh+0x32C8) = -1, game rebuild `BuildActiveVehiclePointerArray` (0x42BC80, cdecl, no args, returns active count), `VehicleCount` published only if the rebuild returned exactly the expected count. Rollback on any mismatch: roles restored, rebuild, saved array written back, count untouched.
4. The survivors get a reduced order (renumbered 1..K) and the existing `write_grid` reorders and fires the mini-restart.
5. Restore in `prequal_tick` (worker loop): only when session 7, RacePhase 8 (DONE), GameState 5 or 7, with the exact reduced-session proof (epoch, count, pointers). Order: roles back, full ordered array, `VehicleCount` last. If the session ends before phase 8, the state is released WITHOUT writing game memory.

## Refusals (all log to grid_restore_log.txt, all leave the full field)
- header absent / malformed / repeated / out of range
- career_weekend.flag absent
- collect_field or plan_grid mismatch
- field within the limit (nothing excluded)
- fewer than two starters would remain
- rebuild count mismatch (rollback)
- restore proof failure (state dropped, excluded cars stay out, no memory written)
- grid refused after a committed exclusion (restore of the exclusion, pre-start proof)

## Player DNQ: not enforced (pending)
The Red Flag player gate (detour at 0x4D10E0, garage-pose lock, gr44.c ~L2272 and ~L3745) is NOT ported. A player planned outside the limit stays in the field and the log says `player DNQ: not enforced (pending)`. Spec design item 4 allowed this fallback.

## Deviations from Red Flag (deliberate)
- Restore does not write FINISHSTATE/REASON/LAP/RACEPOS or RetiredFlag: the excluded cars never started, so Red Flag's DNF writes do not apply.
- Restore trigger is phase 8, not phase 3/5 (Red Flag restores there on its own DNF path). Phase legend from gr44.c L1227 (2=GRID_READY, 3=GREEN_FLAG, 5=FINISHING per that comment, but live yellow races read 5/5 per gr44.c L1202). Phase 8 as the finished phase is NOT live-verified.
- Rollback restores the saved array explicitly (Red Flag only re-runs the rebuild).

## Verification performed
- `career_prequal` scratch test (header parse, refusals, DNQ selection, player kept as P27, reduced order, plan on kept names): PASS. Scratch only: `...\scratchpad\hG\prequal_test.cpp`. NOT added to the build.bat harness.
- Existing harness with `-Wall -Wextra -Werror`: `policy_test` PASS, `apply_test` PASS (run in scratch, not in the workspace).
- build.ps1 into `trials\build_20261021_PREQUAL`: rc 0. No warnings from `grid_restore.cpp` or `career_prequal.hpp` in `compile.log`. Exports LightloaderPluginInit and RaceDataCollectorStop present.

## Not verified (open)
- Native: nothing executed in the game. The exclusion, rebuild, and restore have NOT been exercised against live memory.
- Whether the mini-restart (0x7CE25D) rebuilds `VEHICLE_COUNT` again. Assumed not (spec design item 3).
- Whether phase 8 occurs in the race and whether GameState stays 5 or 7 there.
- The freeze path (`enforce_freeze`, 00:00 snapshot) is NOT covered. Only the career_grid.tsv path.
- Lightloader source freeze already writes `#starters` (`release_2.3.25_prep_2026-10-13\source_freeze\career\Core\Rules\CareerWeekend.cs` L211). Confirm the deployed Lightloader writes it too.

## Pending validation (owner)
Spec design section "Validation": 1984 Senna round with 29 cars, qualifying to 00:00, CONTINUE: race grid shows 26 cars. Then a forced player DNQ run. Human validation required.

## Environment notes (pre-existing, not changed)
- `tools\workspace\check-workspace-structure.ps1` fails on this machine: validator root `%USERPROFILE%\Desktop\F1` does not match `E:\F1 Challenge 99-02\Project\F1`, plus missing paths. Not run as a pass.
- `tools\f1c_mod.py evidence-query findings` crashes on a malformed line in `f1c_ida_decompiled\findings.jsonl` (JSON invalid escape). Prior art was checked by grep instead.
- MinGW g++ does not run from the bash tool here (exit 1, no output). Compiles were run through PowerShell with `C:\msys64\mingw32\bin` on PATH.

## Restoration
- Restore the previous source: copy `grid_restore.cpp.BEFORE_20261021_prequal` back over `grid_restore.cpp` and delete `career_prequal.hpp`, or use the previous DLL `trials\build_20261020_RUNTIME2`.
- No game files were touched. No product status changed.

## Follow-up
- Update `README.md` in this folder (not edited here: outside the task's file list).
- Owner native test per the spec, then decide on the freeze path and the player gate port.
