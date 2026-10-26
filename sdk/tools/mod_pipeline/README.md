# F1C Mod Investigation Pipeline

Turns "understand behavior X, then change it" into a repeatable 4-stage flow.
Built for surgical modding using the reverse-engineering map, not blind memory poking.

## Start here

**Doing a behavioral mod? Read `MODDING_PLAYBOOK.md` first.** It has the exact
process, the iron rules (trust tiers, patch a copy, verify bytes, check stack
balance), and a fully reproducible worked example.

Two deterministic helpers remove all hand-disassembly (`asm_tools.py`):
- `python asm_tools.py resolve <addr>` — one-shot function dossier: confirmed
  identity, string anchors, callers, and `ret imm` (calling convention / arg count).
- `python asm_tools.py redirect <caller> <from> <to> [--at <va>]` — finds E8 call
  sites, computes retarget bytes, and checks stack balance (pinpoints the `push`
  to NOP if arg counts differ).

## Stages

| Stage | Command | Output |
|-------|---------|--------|
| SEED  | write `seeds/<name>.json` | behavior spec: anchor globals, seed funcs |
| MAP   | `python mod_pipeline.py map seeds/<name>.json` | `investigations/<name>/dossier.md` + `consumers.csv` |
| WATCH | `python mod_pipeline.py watch seeds/<name>.json` | `investigations/<name>/<name>_watch.js` (Frida) |
| PATCH | `python mod_pipeline.py patch --addr 0x.. --type float --value ..` | `patches/<name>.json` + `<name>.js` |

Python: `%USERPROFILE%\AppData\Local\Python\bin\python3.exe`

## Seed spec format

```json
{
  "name": "ai_drying",
  "description": "one-line problem statement",
  "globals": [
    { "addr": "0x008afee8", "label": "OnPathWetness", "type": "float" }
  ],
  "seeds": ["0x00404250"],
  "player_ranges": [[start, end]],   // optional: force path_class=player_physics
  "ai_ranges": [[start, end]],       // optional: force path_class=ai
  "struct_watches": [                // optional runtime per-object reads
    { "base_global": "0x005f0dec", "index": 0, "stride": 18624,
      "offset": 368, "type": "float", "label": "player_grip" }
  ],
  "watch_hz": 10
}
```

## Trust tiers (baked into every dossier)

- **FACT** — raw `.c` source lines, shown inline. The only thing to trust for precision.
- **GUESS** — docs/12 global labels, docs/13 offsets. DS-worker inferred; verify.
- **LEAD** — docs/31 call chains, semantic names. May be hallucinated (proven: chain 0008).
- **CONFIRMED** — ISI class from NASCAR cross-binary match (`cross_binary_final.csv`).

The PATCH stage reads original bytes from the exe — use it to verify a global's real
value/label before trusting docs/12. (Caught `0059e450` = 0.5, mislabeled "DegreesToRadians".)

## Data sources (pre-existing)

- `tools/ida_data_xrefs.csv` — 17,037 func->global->read/write edges (the consumer index)
- `tools/cross_binary_final.csv` — confirmed NASCAR/ISI identity
- `knowledge_graph/callers.json` — inverse call graph
- `analysis_pipeline/analyzed/*.c` — raw decompiled source (ground truth)

## Workflow

1. Name the observable, pick the anchor global(s) that represent it.
2. `map` — read the dossier. The player-path vs AI-path split shows where behavior diverges.
3. Read the inline FACT source of the diverging functions. Find the constant/branch.
4. `watch` — generate a Frida agent, run a live race, confirm causation before touching bytes.
5. `patch` — once proven live, bake the fix as an exe byte-patch (rollback bytes saved).

## Validated case: ai_drying

`AI_grip = 1.0 - mean(OnPathWetness, OffPathWetness)` — single track-global scalar,
recomputed in staged batches by `FUN_00404460` when wetness crosses `DryingThreshold`.
Separate from the player's per-tire wet model. See `investigations/ai_drying/dossier.md`.
