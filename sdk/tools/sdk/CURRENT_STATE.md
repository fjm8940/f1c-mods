# F1C SDK — current state

**As of 2026-09-30.** This page is the short "what is true now" view. `README.md`
keeps the dated history. For machine-readable state, use
`python tools/f1c_mod.py start --json` and `sdk-info --json`. Update this page
when an interface, target, gate or objective changes.

## Identity

| Item | Value | Source |
|---|---|---|
| Public SDK version | 1.0.0 | `F1C_TOOL_CONTRACT.json`, `f1c_sdk_version.h` |
| Broker ABI | 15 | `f1c_broker_abi.h:7` (authoritative; `sdk-info` reads it) |
| Broker distribution | Built into Lightloader 2 `dinput8.dll`; the public catalog also lists `shared_broker` 15.0.0 | `development/lightloader/v2.0.0-hub-dev.1/BROKER_MERGE_SPEC_2026-09-29.md`, `production/lightloader/v2.0.1/catalog.json` |
| Target | Fixed-address canonical EXE, SHA-256 `1A70E5ED…`; variants through `variant-profile`/`resolve-symbols` | `f1c_target.h` |
| Catalogue | 6,383 functions (47 reviewed ABIs, the rest LEGACY leads), 113 vehicle fields, 165 legacy structs, 181 vtables | `sdk-query stats` |
| Test gate | `run_all.bat`, 60 suites; the full default `sdk-test` is the definition of done | `tests/run_all.bat` |

## Knowledge stores

| Store | State |
|---|---|
| `findings.jsonl` (curated prior art) | 602 entries after the 2026-09-30 S1/S6 backfill: September ABIs, live crash evidence and disproven approaches added with explicit tiers (ids `S1-*`, `S6-*`). Query with `evidence-query findings TEXT --json`. |
| Checkpoint dated entries | Surfaced as `RECORDED` (unreviewed) in `evidence-query findings` |
| Raw Hex-Rays corpus `functions/` | July 2026 export; preserved evidence |
| Typed corpus `functions_typed_20260930/` | 7,132 functions from the reviewed lab IDB with reviewed SDK types and names applied (17 names corrected versus raw); `MANIFEST.json` gives provenance |
| Typed lab IDB | `analysis/sdk_lab_idb/F1_Challenge_Online_sdk_lab_typed_20260930.i64`; default source for `ida-query` |

Hex-Rays prints struct offsets in **decimal, scaled by the pointer's element
size**. Byte offset `0x4620` appears as `+ 17952` on a byte/char pointer and as
`[4488]` or `+ 4488` on a `_DWORD*` pointer. To find all readers and writers
exactly, use `ida-query xrefs-to` or `evidence-query offset`.

## Front-door tooling (2026-09-30)

- `start` / `sdk-info` / `doctor`: freshness, conflicts and check coverage.
- `evidence-query findings TEXT --json`: prior art, curated plus `RECORDED`.
- `inspect ADDRESS_OR_NAME`: raw, typed, facts and derived sources, plus SDK
  helper links.
- `search`: ripgrep-prefiltered, with results identical to a full scan.
- `ida-query {info,decompile,disasm,xrefs-to,xrefs-from,bytes} ADDRESS`:
  read-only, on a disposable IDB copy (idalib).
- `ida-export --output DIR --save-idb PATH`: regenerates a typed corpus after
  new reviewed types or names.
- `sdk-test [--suite tooling | --no-regenerate]`.

Details: [LLM_OPERATING_CONTRACT.md](LLM_OPERATING_CONTRACT.md).

## Bottleneck

Almost every pending SDK capability has passed its deterministic and
native-byte tests and waits for **native gameplay acceptance**. Examples:
session exit/re-entry, replay roster, restart fuel and penalties, pit voice,
and ESC actions (checkpoint `pending` fields, 2026-09-15…23). A human must
currently reconstruct each scenario by hand.

## Next objectives

The authoritative list is `next_objective` in `AUTOMATION_CHECKPOINT.json`,
refreshed 2026-09-30.

1. Resolve the `field_telemetry` protected-initialization failure, or retire it
   from player packages, before live ABI-11 telemetry validation.
2. Add timing, rules, track identity, camera and input snapshots.
3. Promote reviewed lap, pit, damage, failure and race-control polling events.

**Deferred by the owner (2026-09-30), not to be started without a request:**
native testing stays in person for now.

- The guarded scenario runner. Its plan is kept in
  [SCENARIO_RUNNER_SPEC_2026-09-30.md](SCENARIO_RUNNER_SPEC_2026-09-30.md).
- The extra-Safety-Car same-process re-entry crash fix.

Both are listed under `deferred_objectives` in the checkpoint.
