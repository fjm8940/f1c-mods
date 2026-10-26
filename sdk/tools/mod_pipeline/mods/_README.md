# tools/mod_pipeline/mods/ — Pipeline-Authored Mods

Mods produced by the modding pipeline. (Per `work/PLAN_reorg.md` Phase 2, these
consolidate into the top-level `mods/` so all mods live in one place. Until then,
this is where pipeline-generated mods land.)

| Mod | What | Status |
|-----|------|--------|
| `cut_track_dq/` | Cutting the track = instant disqualification | **in-game confirmed** |

Each mod dir contains:
- `apply_<mod>.py` — writes a patched COPY of the exe (verifies original bytes, never touches original)
- `<mod>.js` — Frida runtime version (reversible, has `restore()`)

Pattern and rules: `../MODDING_PLAYBOOK.md`.
