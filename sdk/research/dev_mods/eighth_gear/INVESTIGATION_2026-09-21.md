# Investigation record — Eighth Gear status review

Date: 2026-09-21
Trigger: owner request ("look into the 8th gear mod we've created")
Type: read-only review; no files changed in the game installs

## Product summary (verified from repository records)

- Product: F1C Eighth Gear, `development\mods\eighth_gear`, version 0.1.2-dev.
- Mechanism: configuration-only overlay. No DLL, injector, or executable patch.
- Targets `F1 Challenge V10` vehicle HDV files and saved setups. Source game
  directories are read-only inputs; builder refuses to overwrite outputs.
- Pipeline stage: human validate. Promotion status: not authorized.
- Production: not present under `production\mods` (verified 2026-09-21).
- Governance: not listed as its own entry in `governance\PRODUCT_STATUS.json`;
  falls under the `other-dll-mods` portfolio entry, although it is a
  configuration mod rather than a DLL mod.

## Version history (verified)

| Version | Scope | Outcome |
|---|---|---|
| v0.1.0 | 51 seven-speed HDVs, eighth ratio from stock catalogue | Human test "no 8th gear"; car misidentified at first |
| v0.1.1 | Ferrari 2005 only, `Gear8Setting=73` (`1.000000`) | Installed on clean test; human result never recorded |
| v0.2.0 | Six-speed expansion, adds ratios 0.950/0.900/0.850 | Withdrawn — premise invalidated (test car was Ferrari, not Toyota) |
| v0.1.2 | Adds explicit `Gear8Setting=73` to `tempGarage.svm` and `2005RH Australia.svm` | Installed on clean test; human result never recorded |

Owner-corrected root cause of the observed failure: the Ferrari 2005 test car
already selected eighth gear; the v0.1 ratio `1.142857` caused extreme RPM.
The v0.2 six-speed expansion does not address this and must not be installed
as a fix.

## Current target-state check (2026-09-21)

- Live install `F1 Challenge V10`, Ferrari 2005 HDV
  (`SeasonData\Vehicles\Ferrari\2005RH_F2005\2005_Ferrari.hdv`):
  `ForwardGears=7`, `Gear6Setting=55`, `Gear7Setting=61`, no `Gear8Setting`.
  SHA-256 `CA5F2ACD2635239FDBACE61266107BEA29AEE75B152FE95EA9C9C8F9B6D3741C`.
  The live installation is stock/untouched by this mod, consistent with its
  recorded "read-only input" role.
- `F1 Challenge V10_clean` (the recorded installation target for v0.1.0,
  v0.1.1, and v0.1.2) does not exist at the workspace root, on the Desktop,
  or as a top-level directory on any mounted drive. The installed
  `tempGarage.svm` and `2005RH Australia.svm` files are also absent from the
  workspace. Cause unknown; not investigated further to avoid destructive or
  speculative actions.
- Result of `tools\workspace\check-workspace-structure.ps1` (2026-09-21):
  8 errors, 0 warnings. New vs. the August records:
  - `Required top-level directory missing: F1 Challenge V10_clean`
  - `Canonical product path missing (cleanTestGame): F1 Challenge V10_clean`
  - `Unexpected top-level directory: .pytest_cache`
  - `Unexpected top-level directory: trials`
  Pre-existing and unchanged: 4 source-like files under
  `production\bug_report_site\v1.0.0` and `v1.0.1` server paths.

## Consequence for this product

- The v0.1.2 human-validation gate cannot be executed in its recorded form:
  both the target installation and the installed saved-setup files are gone.
- All eighth-gear install evidence remains valid as history (hashes and
  restoration maps preserved in `testing\eighth_gear\` and the trial trees),
  but the before-state files to restore no longer exist.
- No promotion, production copy, or live-game change was ever performed, so
  no shipped artifact is affected.

## Documentation inconsistency found

`README.md` lines 14-21 describe the six-speed expansion and the added
`0.95`/`0.90`/`0.85` ratios as current scope. That is the withdrawn v0.2.0
behavior; `STATUS.md` correctly records v0.2 as withdrawn and the active
correction as v0.1.2 (HDV + saved-setup `1.000000` fix). README needs an
owner-approved correction before it is used as the entry narrative.

## Recommended next steps (owner decision required)

1. State whether `F1 Challenge V10_clean` was intentionally removed/moved or
   is missing unexpectedly. If intentional, the eighth-gear test path needs a
   new clean target and a new before-state/restoration record.
2. If restored, repeat the guarded Australia/Ferrari full weekend and monitor
   the 7th-to-8th RPM change and engine survival (unchanged next gate).
3. Decide whether to correct `README.md` scope text and whether to give the
   product its own `PRODUCT_STATUS.json` entry.

## Evidence tier

- Verified now: Ferrari live-install HDV values/hash, absence of clean install
  and saved-setup files, validator result, production absence.
- Verified from preserved records: version history, hashes, installation and
  restoration details.
- Inference: none relied upon beyond clearly labelled cause-unknown items.
