# P2 oracle build record (2026-10-16)

Scope: the Python reference of the P2 decision logic (`docs/P2_APPLY_SPEC_2026-10-16.md` sections 1-2) and its unit tests.
No game, no C, no DLL, no install. No existing file was modified.

## Created (new files)
- `tools/aad_decide.py`: oracle. Functions `model_key`, `adaptive_step`, `weekend_start`, `lock_level`, `race_update`; `TRACK_ALIAS`, `MODEL`; read-only CLI `start --store PATH --track KEY --slider N`.
- `tools/test_aad_decide.py`: 26 unittest cases. Run: `python -I -B tools\test_aad_decide.py` (from the mod folder). Result 2026-10-16: 26 OK.
- This record.

## Verified (evidence)
- Unit tests: 26/26 pass. Covers alias case-insensitivity, unknown-track fallback (S = -0.307 x lap), refusals (teammate laps, missing fields, n below minimum, CI width), hand-computed Austria case, weekend tiers native/overall/track, lock_level (ladder, practice accepted with and without track rating, practice refused), race_update accepted and refused, CLI (JSON output, store file unchanged, no new files).
- Equivalence: 200 random summaries against `aad_apply.adaptive_step` (min_laps 5, max_ci 1.0, teammate reference, alpha random): factors and computed values identical; 30 accepted, 170 refused.
- Hand values (checked by hand and by the module): Austria gap +0.5 s, lap 72.0, current 1.0 -> computed 0.975, factor 0.988. Unknown track same inputs -> computed 0.968, factor 0.984. Overall tier (two tracks at -1.0 %) -> 1.009. Practice accepted, gap +1.0 s -> 0.958; with track rating 1.046 -> smoothed 1.002.
- Tests write only under `%TEMP%` (store file for the CLI test); no store or ini in the game folder.

## Spec ambiguities and the choice made
1. Return order of `adaptive_step`: the spec says `(factor, computed, why)`; `aad_apply` returns `(new, detail, computed)`. Chose the spec order. Refusal returns `(None, None, why)`.
2. `race_update` third-field name `updated`: chose a bool meaning "race accepted" (factor changed to the adaptive result). The race is recorded in both cases.
3. Store key: `weekend_start`, `lock_level` and `race_update` use the raw `track` as the store key (spec text), while `model_key` only selects the lever model. So the store must be keyed by the AIW stem (`2001_A-1_Ring`), not the model key (`austria_2001`). A mismatch silently gives the native tier.
4. `model_key` returns unknown names unchanged. A bare model key (`austria_2001`) therefore finds its model entry, and `austria_2001_wet` finds the wet entry (S -35.0). The spec says the wet entry is not used in P2. Not fixed (spec is literal); flagged for the owner or the C port.
5. Rounding tie: `smooth(1.0, 0.975, 0.5)` = 0.9875 exactly in decimal. Python's `round(.., 3)` gives 0.988 because the binary value is just above the tie. The C `aad_round3` must reproduce this (test `AdaptiveStepAustria` pins 0.988). Recommend the C harness include this case.
6. `n_player_clean` check: spec says `< min_laps` on the value; `aad_apply` uses `int()`. Identical for integer thresholds; the oracle uses the `int()` form.
7. CLI with a missing `--store` file: the spec is silent. Chose to print an error and exit 2 instead of silently returning the native tier. `rs.load` on a missing file would otherwise return an empty store.

## Not done / pending
- Not modified: `docs/DESIGN_DECISIONS.md` (DD-12 still says "Not implemented"; DD-13 ladder is in `aad_ratings.lookup`), mod `README.md` (no index entry for this record), `docs/HANDOFF_2026-10-16.md`. Owner or next agent should decide whether to add the index line and the DD-12 status update.
- C port (`dll/aad_decide.c`), harness modes and C-vs-oracle tests: not in this task (spec section 6, agents B and D).
- Workspace validator (`tools/workspace/check-workspace-structure.ps1`): 2 errors, pre-existing and unrelated to this work: unexpected top-level directories `.claude` and `plans`. Not changed (structural; owner approval needed).

## Restoration
Remove the two new files in `tools/` and this record. No existing file was edited, so nothing else needs restoring.

## Wasted-effort check
Repeated reads: none material. Failed commands: validator first called with backslash path (retried with forward slashes); a `-c` check needed an explicit `sys.path` insert under `-I`; ResourceWarnings in the CLI test fixed with context managers.
