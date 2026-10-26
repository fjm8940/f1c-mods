# Text-corruption report — 2026-09-30

Report only; no files were modified.

Scope: `*.md` under `f1c_ida_decompiled/tools/sdk`, `development/mods`, `operations/release_management` (relative to the workspace root).
Method: ripgrep 15.1.0 `--count-matches` per pattern; examples are match-centred excerpts of at most 160 characters.
Interpretation: pending strong-model review.

## Pattern — mojibake

Pattern: `â€|Ã[\x80-\xBF]|Â[ -~]`

Total: 310 matches in 22 files.

Top files:

- 40 `development/mods/full_safety_car/README.md`
- 40 `development/mods/full_safety_car/trials/v0.10.64_replay_ownership/install-20260922T112332Z/before-install/document-1-README.md`
- 40 `development/mods/full_safety_car/trials/v0.10.65_hud_restart/documentation-before/component-README.md`
- 40 `development/mods/full_safety_car/trials/v0.10.65_hud_restart/install-20260922T115752Z/before-install/component-README.md`
- 40 `development/mods/full_safety_car/trials/v0.10.66_replay_live_field/before-shared/0-README.md`
- 40 `development/mods/full_safety_car/trials/v0.10.66_replay_live_field/install-20260922T122311Z/before-document-0.md`
- 10 `development/mods/full_safety_car/CURRENT_STATUS.md`
- 7 `development/mods/full_safety_car/trials/v0.10.64_replay_ownership/install-20260922T112332Z/before-install/document-2-README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.66_replay_live_field/before-shared/1-README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.66_replay_live_field/install-20260922T122311Z/before-document-1.md`
- 7 `development/mods/red_flag/README.md`
- 7 `development/mods/red_flag/trials/2026-09-22/replay-panis/component-README.before.md`
- 7 `development/mods/red_flag/trials/2026-09-22/second-rf-restart/before/README.md`
- 7 `development/mods/red_flag/trials/2026-09-22/second-rf-restart/install-20260922T114515Z/before-install/component-README.md`
- 3 `development/mods/mechanical_failures/docs/MECH_TEST_GUI_LIVE_FIXES_2026-09-27.md`
- 2 `f1c_ida_decompiled/tools/sdk/DEEPSEEK_HANDOFF_2026-09-30.md`
- 1 `development/mods/full_safety_car/trials/v0.10.64_replay_ownership/README.md`
- 1 `development/mods/full_safety_car/trials/v0.10.64_replay_ownership/ended-message/trial-README.before.md`
- 1 `development/mods/full_safety_car/trials/v0.10.64_replay_ownership/install-20260922T112332Z/before-install/document-0-README.md`
- 1 `development/mods/red_flag/trials/2026-09-10/coordinator-v78-r2/README.md`

Examples:

- `development/mods/mechanical_failures/docs/MECH_TEST_GUI_LIVE_FIXES_2026-09-27.md:16` — - **Error text was mojibake** (`'MÃ¤Ã¤ritettyÃ¤...'`): the SDK fron...
- `development/mods/full_safety_car/CURRENT_STATUS.md:87` — **Installed v0.10.49 â€” ESC results revision:** [v0.10.48 native failure, corrected pre-simulati...
- `development/mods/red_flag/trials/2026-09-22/second-rf-restart/before/README.md:136` — ## Published v1.1.0 â€” 2026-09-07

## Pattern — digits glued to words

Pattern: `\b(?:at|Exact|Published|SHA256|and|to|of|with|from|slot|role|than)\d`

Total: 595 matches in 206 files.

Top files:

- 20 `development/mods/red_flag/trials/2026-09-22/massa-stuck-r4/SC_LOCATION_INVESTIGATION.md`
- 13 `development/mods/red_flag/trials/2026-09-22/replay-roster-guard/reviewer/REVIEW.md`
- 12 `development/mods/full_safety_car/MALCHAREK_DEEP_REVIEW_2026-09-10.md`
- 11 `development/mods/full_safety_car/trials/v0.10.56_fault_redflag/LIVE_TRULLI_WAIT_2026-09-15.md`
- 11 `development/mods/red_flag/trials/2026-09-15/retired-object-graph/README.md`
- 11 `development/mods/red_flag/trials/2026-09-21/policy-v89/completion-work/penalty-hold/README.md`
- 10 `development/mods/full_safety_car/CURRENT_STATUS.md`
- 8 `development/mods/full_safety_car/GENE_CAUTION_LIVE_2026-09-08.md`
- 8 `development/mods/full_safety_car/README.md`
- 8 `development/mods/pitstop_modding/docs/TUNING_IMPLEMENTATION_PLAN_2026-09-21.md`
- 8 `development/mods/red_flag/trials/2026-09-15/local-crash-161449/README.md`
- 7 `development/mods/full_safety_car/trials/live_grid_20260910_2030/README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.31_sim_time_r2/README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.54_player_clearance/README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.64_replay_ownership/install-20260922T112332Z/before-install/document-1-README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.65_hud_restart/documentation-before/component-README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.65_hud_restart/install-20260922T115752Z/before-install/component-README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.66_replay_live_field/before-shared/0-README.md`
- 7 `development/mods/full_safety_car/trials/v0.10.66_replay_live_field/install-20260922T122311Z/before-document-0.md`
- 7 `development/mods/red_flag/trials/2026-09-22/massa-stuck-r4/IMPLEMENTATION.md`

Examples:

- `operations/release_management/safety_car/CLK55_1_0_1.md:5` — Published1.0.1 at https://mods.palloset.com/clk55, immutable /downloads/clk55/v1.0.1/clk55-v1...
- `operations/release_management/safety_car/RELEASE_2026-09-14.md:18` — rts at boot. Two simultaneous downloads and20MiB maximum file size bound payload buff...
- `operations/release_management/safety_car/customer_incident_20260915/full-log/README.md:8` —  subsequent load: access violation16616 at0054C044 reading40BE...

## Pattern — words glued to digits

Pattern: `\d{2,}(?:byte|passed|tests|race|races)\b`

Total: 3 matches in 3 files.

Top files:

- 1 `development/mods/full_safety_car/AUTO_ACTIVATION_DIAGNOSTICS_2026-09-10.md`
- 1 `development/mods/full_safety_car/trials/release_security_audit_20260914/README.md`
- 1 `development/mods/full_safety_car/trials/v0.10.53_restart_exclusion/installation-173738/README.md`

Examples:

- `development/mods/full_safety_car/trials/release_security_audit_20260914/README.md:40` — 2313/2388 original .text64byte blocks are byte-identical at the sa...
- `development/mods/full_safety_car/trials/v0.10.53_restart_exclusion/installation-173738/README.md:5` — Installed exact protected8755712byte candidate SHA25...
- `development/mods/full_safety_car/AUTO_ACTIVATION_DIAGNOSTICS_2026-09-10.md:5` — , writes memory, injects or hooks. Exact444byte ABI1 and PACE magic checke...

