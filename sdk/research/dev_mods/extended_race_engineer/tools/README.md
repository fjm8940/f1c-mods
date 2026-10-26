# ERE tools - voice index

The voice index is the main tool for the Extended Race Engineer mod. It shows every
race-engineer clip, whether vanilla, whether another mod already plays it, and the
ERE status and technique. It replaces the archived EVF `progress.py` and
`voice_browser.py` (EVF stays read-only history).

## Run

    tools\browse_voices.bat            (or: python voice_browser.py)
    python voice_index.py counts       (headless summary)
    python voice_index.py export       (writes ..\docs\VOICE_INDEX.md)
    python voice_index.py merge-audit  (writes the audit into ..\data\voice_index.json, keeps .bak)
    python voice_index.py check-audit  (self-check of the audit merge; exit 1 on a problem)
    python voice_index.py seed --force (re-seed from EVF progress.json; keeps .bak)

Tests (headless, no game, no ffplay):

    python -I tests\test_voice_index.py

## Files

| File | Role |
|---|---|
| `voice_index.py` | Data layer. Load/save, seed, merges, filters, export, SDT reader. No tkinter. |
| `voice_browser.py` | tkinter GUI over `voice_index.py`. Dark theme, sortable list, filter chips, detail pane, preview. |
| `browse_voices.bat` | Launcher: checks Python and ffplay, runs the GUI from this folder. |
| `..\data\voice_index.json` | **Source of truth.** Owner-edited. Schema `ere.voice_index.v1`. |
| `..\data\modded_lines.json` | Census of lines other mods play. Written by another worker. Schema `ere.modded_lines.v1`. Optional. |
| `..\data\design\*.json` | Per-batch ERE trigger designs. Schema `ere.design.v1`. Optional. |
| `..\docs\VOICE_INDEX.md` | Generated view of the index. Do not edit by hand. |

## Row fields (data\voice_index.json, keyed by SDT index)

- `sdt_index`, `clip`, `sdt_name`, `category`, `description` (the "says" text), `eids`, `blocked`, `orphaned`
- `vanilla` (bool) from the old EVF SHIPPED status; `vanilla_source` shows whether that
  came from a user triage or an auto classifier. Auto SHIPPED means reachable, not heard.
- `evf_status`, `evf_source`: the old EVF status, kept for history
- `modded_by`: list of `{mod, mod_version, eid, mechanism, condition, source, release_status, live_verified}`.
  Entries with `source: "seed"` come from the seed (EVF DONE rows and the 35 MF engineer clips).
  All other entries are recomputed from `modded_lines.json` on every load.
- `ere_status`: one of `NONE`, `PLANNED`, `BUILT`, `ISOLATION_OK`, `LIVE_OK`, `WONT_DO` (owner-set)
- `note` (owner note), `triage` (owner triage from EVF), `settings` (SDT path and how it was chosen)

## Audit field (tool-owned)

`audit` on each row comes from `..\data\audit\B*.json` (schema `ere.audit.v1`, `*.BEFORE_*` backups ignored):
`batch, eid, verdict` (NATIVE_ACTIVE / MOD_ONLY / NOT_ACTIVATED / UNKNOWN), `reason, situation, mod,
mod_active_in_owner_game, suppressed_trigger_exists, suppressed_note, tier, play_plan` (summary keys:
meaning, when_it_should_play, trigger, subject, technique, latch_cooldown, must_not_play_when, live_test,
confidence) and `stitch_recheck` (old/new verdict, why). Recomputed on every load and by `merge-audit`;
never edit it by hand. Owner fields (note, ERE status, triage, description, ...) are never written by the merge.
Extra filter chips: Native active, Mod only, Not activated, Unknown, Suppressed.

## Buckets and filters

- **Vanilla**: `vanilla` is true.
- **Modded**: not vanilla and `modded_by` is not empty.
- **To-do**: not vanilla and not modded. This is the real work list. The ERE status column
  still shows in-progress work on these rows.
- Filter chips: All, Not vanilla, To-do, Modded, ERE live-ok.

## SDT (read only)

Preview needs `PC_RE_MAIN.SDT` (316 clips). Lookup order:

1. the path remembered in `settings.sdt_path`
2. `%USERPROFILE%\Documents\F1 Challenge 99-02 (V10)\Audio\Commentary\Pws\PC_RE_MAIN.SDT`
3. `%USERPROFILE%\Documents\F1 Challenge V10\Audio\Commentary\Pws\PC_RE_MAIN.SDT`
4. a file picker. Your choice is remembered (`settings.sdt_path_source` = `picked`).

The 35 rows at SDT 316-350 (`PWS_MF01`..`MF35`) are not in the vanilla SDT. The mechanical_failures
mod adds them, so preview is unavailable for them until that mod's SDT is the one you point at.

## Saving

Save writes a temp file, copies the old file to `voice_index.json.bak`, then replaces the file.
Closing with unsaved changes asks first. The index is never hard-deleted.

## Limits

- Live-verify states (`ISOLATION_OK`, `LIVE_OK`) are owner-set. The tool never sets them.
- `modded_lines.json` is not in the repo yet. Until it exists, the modded column shows only seed entries.
- The GUI has not been launched in this pass; only the headless tests ran (14 pass).
