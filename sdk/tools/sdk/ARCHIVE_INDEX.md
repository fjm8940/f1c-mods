# Preserved archive index

Nothing listed here is deleted or stripped of historical detail. These trees
are archived in place and must be approached through
`python tools/f1c_mod.py route PATH`.

| Area | Status | Supported use |
|---|---|---|
| `functions/`, `functions_annotated/` | immutable decompiler evidence | `search`, `inspect` |
| `skeletons/`, `skeletons_filled/` | immutable facts and derived prose | `search`, `inspect` |
| `analysis/`, `runtime_structs/` | generated/static/runtime evidence | memory, runtime, variant commands |
| `investigations/`, `docs/` | historical experiments and research | evidence leads only |
| `mods/sdk/` | compatibility SDK export | old builds only |
| `mods/*` except `mods/sdk_sandbox/` | prototypes and production history | read-only evidence |
| `shadow_source/` | completed static recovery and worker history | evidence only |
| `replacement_engine/` | paused engine-recreation experiment | preserved, no current work |
| `tools/*` except `f1c_mod.py` and public SDK files | internal implementation | never direct public invocation |

Top-level handoffs, prompts, roadmaps, and old tool catalogues are snapshots of
earlier phases. They may contain valid facts and obsolete commands. The current
contract and automation checkpoint supersede their instructions, not their
evidence.
