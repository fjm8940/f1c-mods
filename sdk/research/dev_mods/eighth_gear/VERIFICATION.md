# Verification record — v0.1.0 development trial

Date: 2026-08-25  
Result: **AUTOMATED PASS; HUMAN GAMEPLAY VALIDATION REQUIRED**

## Exact artifact

- Trial: `trials\2026-08-25\v0.1.0-overlay`
- Status: ``
- Tree SHA-256: `01e51f595a061cc8d5ca8b046b4690dd697ca8766c50f04165b8c7a287302aae`
- Source target: local `F1 Challenge V10`
- HDVs scanned: 136
- HDVs modified: 51 seven-speed cars
- HDVs skipped: 85 non-seven-speed cars
- Shared `gear_ratios.ini`: unchanged

The tree hash is SHA-256 over sorted lines of `relative/path sha256`, including
the manifest and `` marker.

## Ratio selection

| Existing 6th | Existing 7th | New 8th | Ratio | Cars |
|---:|---:|---:|---:|---:|
| index 65 | index 69 | index 72 | 1.045455 | 43 |
| index 55 | index 61 | index 67 | 1.142857 | 8 |

The builder projects the existing sixth-to-seventh geometric step and selects
the closest lower ratio already present in the target catalogue.

## Automated evidence

- Focused unit tests: 3 passed.
- Source preservation: all 51 post-build source hashes match the source hashes
  captured in the manifest.
- Output integrity: all 51 output hashes match the manifest.
- Semantic scan: every output has exactly one `ForwardGears=8` and exactly one
  `Gear8Setting`; zero errors.
- Six-speed exclusion: no six-speed HDV appears in the overlay.
- Canonical SDK gate: passed, including C/C++ smoke builds, broker regressions,
  hook-layout compatibility, new-mod workflow, variant resolver, and runtime
  validator.
- Workspace validator after the change: no warnings, with four unrelated
  pre-existing errors under old `production\bug_report_site\v1.0.0` and
  `v1.0.1` server source paths. The same four errors existed at cold start.

## Required human validation

Use the clean/test path and a guarded full race weekend; never QuickRace.

1. Preserve the clean install's complete before-state and record the restoration
   map before installing the overlay.
2. Confirm manual 7→8 and 8→7 shifts under load.
3. Confirm automatic transmission and AI cars use eighth gear naturally.
4. Confirm HUD gear display, sound, RPM matching, and top-speed behavior.
5. Confirm garage gearing controls remain usable and saved setups reload.
6. Confirm qualifying-to-race and session-exit/restart behavior.
7. Restore the clean install and verify equality after removal.

No installation, production copy, or promotion was performed in this task.

## Clean-test installation addendum

The later owner-authorized installation on 2026-08-25 used a clean-specific
overlay rather than reusing the live-game overlay:

- Clean overlay tree SHA-256:
  `7cdbb4179710a3823cf1c123019c904c363e83d45c63a02ab9194f4c6617a17a`
- Target: `F1 Challenge V10_clean`
- Installed and verified: 51 HDVs
- Preserved originals: 51 HDVs
- Backup hash errors: 0
- Installed hash errors: 0
- Full record: `testing\eighth_gear\0.1.0-dev\INSTALLATION_RECORD.md`

The live game installation remains untouched. Gameplay validation is still a
human gate.

## Wasted-effort check

- Repeated reading of large material: one combined gearbox search produced
  truncated output; later queries were narrowed.
- Unnecessarily broad builds/tests: none observed.
- Avoidable failed/retried commands: one PowerShell collection pipeline syntax
  error and one Python test-loader import error, both corrected once.
- Polling/waiting/idle: none observed.
