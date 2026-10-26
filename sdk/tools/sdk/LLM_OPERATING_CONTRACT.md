# LLM operating contract

This is the mandatory cold-start guide for any LLM working in this repository.
There is exactly one supported tool:

```powershell
python tools/f1c_mod.py --help
```

## Start and orient

```powershell
python tools/f1c_mod.py start --json
python tools/f1c_mod.py sdk-info --json
python tools/f1c_mod.py doctor
python tools/f1c_mod.py route PATH
python tools/f1c_mod.py inventory
```

`route` applies the machine-readable `F1C_TOOL_CONTRACT.json` to any repository
path. Immutable and archived files are evidence, not executable instructions.

`start` and `sdk-info` report `freshness` and `recent_checkpoint_entries`. When
`freshness.stale` is true, the checkpoint summary (state, next objectives,
recorded ABI) predates newer dated entries: compare the first objective with the
recent entries before acting and report objectives already completed. `sdk-info`
takes the broker ABI from `f1c_broker_abi.h` and lists any disagreement in
`conflicts`. `doctor` states `checks_run`, `checks_not_run` and `warnings`; a
passing doctor is not a test run or native acceptance.

## Find and understand behavior

```powershell
python tools/f1c_mod.py evidence-query findings "safety car" --json
python tools/f1c_mod.py search "safety car"
python tools/f1c_mod.py inspect 0x42C690
python tools/f1c_mod.py discover MOD_ID
```

Ask for prior art first. `evidence-query findings` joins curated
`findings.jsonl` entries (FACT/CONFIRMED/DERIVED/OPEN/DISPROVEN) with
checkpoint summaries labelled `RECORDED`; `RECORDED` is not a trust tier and
must be verified through its evidence link. `inspect` accepts `0x42C690`,
`0042C690`, `sub_42C690` or a name, bounds neighbours (`--max-neighbors`,
`--expand-graph`) and lists `sdk_links` to current SDK helpers, tests and
reference docs. Those links are navigation only and never raise trust. `search`
prints a stderr note when it must scan the whole corpus. `sdk-query` reports
`total`, `returned` and `truncated`.

Trust ordering is FACT/CONFIRMED, then OBSERVED/DERIVED, then LEGACY leads. An
address is not callable merely because it has a name. A callable contract needs
reviewed calling convention, stack cleanup, owner object, arguments, return
semantics, thread, lifetime, and target identity.

## Build a mod

```powershell
python tools/f1c_mod.py init MOD_ID --title "Title"
python tools/f1c_mod.py validate MOD_ID
python tools/f1c_mod.py generate MOD_ID
python tools/f1c_mod.py build MOD_ID
python tools/f1c_mod.py test MOD_ID
python tools/f1c_mod.py stage MOD_ID C:\path\to\staging
```

For the full local gate use `pipeline`. Edit only the sandbox manifest and
generated project source. Do not copy raw hooks or writes from archived mods.
Discover the packaged broker, negotiate capabilities, and fail closed when a
required capability or exact-target proof is absent.

## Observe and generalize

Use `runtime-observe`, `runtime-validate`, `memory-snapshot`, `memory-diff`,
`memory-build`, `memory-match`, and `memory-correlate` for read-only evidence.
Use `variant-profile`, `symbol-recipes-build`, and `resolve-symbols` before
claiming support for another executable. Never silently reuse fixed addresses
across variants.

Live sessions launch into a complete race weekend through `f1cm_guard.asi`.
QuickRace is forbidden. Human interaction is needed only when the guard cannot
reach the required natural state; do not replace that state with an unsafe poke.

## Maintain the SDK

```powershell
python tools/f1c_mod.py sdk-refresh
python tools/f1c_mod.py sdk-test --suite tooling
python tools/f1c_mod.py sdk-test --no-regenerate
python tools/f1c_mod.py sdk-test
```

`--suite tooling` covers only the front-door tooling. `--no-regenerate` runs
the full suite without rewriting generated SDK artifacts. The default full gate
regenerates them first, prints which ones changed (ignoring the embedded
`generated` timestamp) and preserves changed before-versions under
`analysis/sdk_test_regen_before/`. Only the default full gate satisfies the
definition of done.

Internal helper scripts and batch files are preserved implementation details.
Never cite or invoke them in new instructions. If a capability is missing,
extend `f1c_mod.py`, the reviewed headers/broker, the contract, and enforcement
tests together.

## IDA database questions

```powershell
python tools/f1c_mod.py ida-query xrefs-to 0x4C6E80 --json
python tools/f1c_mod.py ida-query decompile sub_4C6E80
python tools/f1c_mod.py ida-query bytes 0x4C6E80 --size 16 --json
python tools/f1c_mod.py ida-export --output functions_typed_YYYYMMDD --save-idb analysis/sdk_lab_idb/NAME.i64
```

`ida-query` (info, decompile, disasm, xrefs-to, xrefs-from, bytes) runs
idalib against a disposable copy of the newest typed lab IDB. It never opens
or saves the source IDB, and it verifies the source hash afterwards. IDB names
and types keep their own trust tiers; static answers are not runtime proof.
`ida-export` refreshes the typed corpus after new reviewed types or names and
refuses to overwrite. After creating a new export directory, add an
`immutable-evidence` routing rule for it in `F1C_TOOL_CONTRACT.json`. `search` and `inspect` include the newest
`functions_typed_*` corpus as `typed`; the raw `functions/` corpus stays the
preserved July evidence. Hex-Rays prints struct offsets in decimal, scaled by
the pointer element size (byte `0x4620` appears as `+ 17952` or `[4488]`), so
use `ida-query xrefs-to` or `evidence-query offset` for complete answers. IDA
Pro 9.1 with the `idapro` package activated is required; `test_ida_bridge.py`
fails with the activation instruction if it is missing.

## Current state versus history

Each maintained product and the SDK have a short `CURRENT_STATE.md` (SDK:
[CURRENT_STATE.md](CURRENT_STATE.md)). Update it when a public, installed,
candidate or objective state changes. Do not prepend dated news to READMEs;
dated history belongs in the existing logs and records.

## Record evidence

Append every new proven, disproven or open result to `findings.jsonl` with
tier, evidence and source, in addition to any prose report. Never raise a tier
without new evidence. Name planning, review and handoff documents with
`PLAN`, `HANDOFF`, `_REVIEW_` or `ASSESSMENT` so `inspect` does not present
them as reference material.

## Preserve history

Never hard-delete. Records, sources, corpora, findings and capture manifests
are kept; bulk by-products follow the workspace retention policy
(`../../../governance/RETENTION_POLICY.md`) and leave only through its
quarantine. Tests write per-run output to `%TEMP%`. The route contract archives
material in place by classification. Historical commands may explain how evidence was
produced, but they are not authorization to repeat the command. See
`ARCHIVE_INDEX.md`.

## Routine live driver and Safety Car observations

The registered `f1c-memory` MCP runs the canonical `memory-mcp serve` command.
Its tools are `f1c_health`, `f1c_drivers`, `f1c_safety_car`, `f1c_trace`, and
`f1c_compare`. This read-only transport is supported alongside the same CLI:
`python tools/f1c_mod.py memory-mcp health`. If the elevated session reader is
unavailable, use `python tools/f1c_mod.py memory-mcp start-reader`. See
[F1C_MEMORY_MCP.md](F1C_MEMORY_MCP.md) for session lifetime, target checks,
freshness, interpretation limits and verified versus pending tests.
