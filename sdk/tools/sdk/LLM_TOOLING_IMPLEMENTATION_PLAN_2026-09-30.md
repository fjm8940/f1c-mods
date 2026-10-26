# LLM tooling implementation plan — 2026-09-30

**Status:** canonical execution plan. D1–D9, E1–E4, G1–G3 done and accepted 2026-09-30 (see log); mechanical batch closed; polish and instruction enforcement done; S1–S4, S6, S7 and O1, O3–O5 done 2026-09-30; S5 specified and DEFERRED by owner 2026-09-30 (testing stays in person); O2 declined by owner (see log).
**Supersedes for execution order:** the three same-day documents below. They stay
as rationale and evidence; nothing in them is rewritten.

| Source | Role now |
|---|---|
| [LLM_WORKFLOW_REVIEW_2026-09-30.md](LLM_WORKFLOW_REVIEW_2026-09-30.md) | Claude measurements (timings, staleness, IDA state) + reconciliation addendum |
| [LLM_WORKFLOW_REVIEW_CODEX_2026-09-30.md](LLM_WORKFLOW_REVIEW_CODEX_2026-09-30.md) | Codex evidence record |
| [LLM_OPTIMIZATION_ASSESSMENT_AND_ROADMAP_2026-09-30.md](LLM_OPTIMIZATION_ASSESSMENT_AND_ROADMAP_2026-09-30.md) | Codex rationale, interface taxonomy, stopping rule for static work |

Implementer handoff for the mechanical subset:
[DEEPSEEK_HANDOFF_2026-09-30.md](DEEPSEEK_HANDOFF_2026-09-30.md).

## 1. Corrections to earlier same-day claims

1. `findings.jsonl` is **not** malformed. Its first 15 lines are intentional `//`
   header comments; `tools/ask.py load_jsonl` skips them. The real finding stands:
   newest dated entry is 2026-08-08.
2. `sdk-test` is **not** read-only. `tests/run_all.bat` first runs five generators
   that rewrite `f1c_sdk.json`, `f1c_sdk.h`, `f1c_vehicle_struct.h`,
   `f1c_vehicle_struct_ida.h`, `f1c_trace_schema.h`, `f1c_reviewed_api.hpp` and
   `analysis/recovery_index.json`. Claude's 12:13 run rewrote them (likely
   byte-identical; unverified).
3. `inspect` address failure is specific to `tools/source_explorer.py
   resolve_address` (exactly 8 hex digits). `tools/ask.py norm_addr` already
   accepts 1–8 digits; the two resolvers disagree.

## 2. Verified code facts the tasks rely on

| Fact | Location |
|---|---|
| `sdk-info` reads `broker_abi_version` (11) from the checkpoint | `tools/f1c_mod.py cmd_sdk_info` (~line 310) |
| Header declares `#define F1C_BROKER_ABI_VERSION 15u` | `tools/sdk/f1c_broker_abi.h:7` |
| `start`/`sdk-info` copy `next_objective` verbatim; no freshness check | `cmd_start` (~349), `cmd_sdk_info` |
| Checkpoint `updated_at` 2026-08-24; 67 dated entry keys up to 2026-09-23, mostly `{state, evidence, pending[, verification]}` | `tools/sdk/AUTOMATION_CHECKPOINT.json` |
| `doctor` checks files/catalogue/tool registry/optional exe only | `cmd_doctor` (~466) |
| `run_all.bat`: 5 generators first; 3 suites listed twice (`test_session_guard`, `test_safety_car_guardrails`, `test_vehicle_lifecycle`); "All canonical SDK tests passed." echoed before ~30 later suites | `tools/sdk/tests/run_all.bat` |
| `search` reads every corpus file in Python (48.9 s); `rg` 15.1 is on PATH for Python (`shutil.which("rg")`) | `source_explorer.search_sources` |
| `inspect` globs all corpora to find one address; JSON embeds full recovery nodes for every neighbour | `inspect_function`, `attach_node_lookup` |
| `sdk-query` truncates then reports `count` = returned | `cmd_sdk_query` (~273) |
| SDK headers carry native addresses as `0x004C6E80u` literals and `sub_4C6E80` comments | e.g. `tools/sdk/f1c_vehicle_native_clear.h:8,38` |
| Tests are plain scripts registered in `run_all.bat`; `sdk-test --suite` already supports focused suites | `tools/sdk/tests/`, `cmd_sdk_test` (~535) |

## 3. Work allocation

Rule: tasks needing evidence judgement, trust-tier decisions, product truth,
live game control, IDA scripting or policy go to a strong model (Claude/Codex)
or the owner. Mechanical, fully specified code changes with deterministic tests
go to DeepSeek.

### 3.1 DeepSeek (mechanical; full specs in the handoff)

| ID | Task | Why safe for DS |
|---|---|---|
| D1 | Test gate: `--no-regenerate`, regeneration drift report with before-copies, dedupe 3 repeated suites, final success message last, `--suite tooling` | Batch/Python plumbing; default behaviour preserved |
| D2 | `inspect`/`resolve_address`: accept `0x` + 1–8 hex and `sub_XXXXXX`, fall back to name lookup | One function; exhaustive test table given |
| D3 | `sdk-info` ABI from header + `conflicts`; `doctor` `checks_run`/`checks_not_run`/`warnings` (never fails on warnings) | Additive JSON fields |
| D4 | Checkpoint freshness block + 5 most recent dated entries in `start`/`sdk-info` (read-only) | Date parse from key suffix; no content decisions |
| D5 | `sdk-query`: `total`, `returned`, `truncated` (keep `count`) | Additive |
| D6 | `search`: ripgrep prefilter with exact-equivalence fallback | Spec pins flags, encoding, exit codes; fixture equivalence test |
| D7 | `inspect`: direct address glob, bounded neighbours, `sdk_links` (headers/tests/docs mentioning the address; no trust change) | Regex + existing recovery keys filter false hits |
| D8 | `evidence-query findings`: include checkpoint entries on the fly as tier `RECORDED` (never written to `findings.jsonl`) | Read-only join; tier explicitly non-reviewed |
| D9 | Text-corruption report (report only, no rewriting) | rg patterns, counts |

### 3.2 Strong model (not DeepSeek)

| ID | Task | Why not DS |
|---|---|---|
| S1 | Tier-reviewed backfill of September findings (FACT/CONFIRMED/DISPROVEN/OPEN) from trial/review records into `findings.jsonl` | Tier judgement; laundering risk; must read evidence critically |
| S2 | Split current state from history for Safety Car, Red Flag and `tools/sdk/README.md` (short current-state view + changelog view; nothing removed) | Requires deciding which installed/public/private identity is current |
| S3 | Typed re-export: propagate reviewed names + `F1CVehicle`/pit types into a **copy** of the lab IDB, Hex-Rays export to a new corpus directory; resolve naming conflicts (e.g. `sub_4C6E80` / `ResetDriverGridState` / native clear) | IDA scripting, conflict resolution, trust semantics |
| S4 | `ida-query` front-door subcommand via idalib (xrefs, struct-offset xrefs, fresh decompile, bytes; read-only copy) | Depends on S3/O4; API design |
| S5 | Scenario runner, first family: session exit/re-entry with extra SC | Live game, guard ASI, safety boundaries |
| S6 | Root cause of space-stripping/mojibake (after D9 report) | Cross-tool investigation |
| S7 | Proposed new `next_objective` list for owner approval | Product priority |

### 3.3 Owner decisions (block the listed tasks)

| ID | Decision | Blocks |
|---|---|---|
| O1 | Disposition of the 11 validator errors (allow-list vs `_human_review`) | green cold start |
| O2 | Checkpoint commits in workspace + SDK repos; untrack `__pycache__` (`git rm --cached`, files stay on disk) | cheap diffs for all agents |
| O3 | Register `f1c-memory` MCP for Claude Code (`.mcp.json`) | Claude live reads |
| O4 | Activate idalib Python package (changes the Python environment) | S3, S4 |
| O5 | Approve S7 objective rewrite of the checkpoint | D4 stale flag clears |
| O6 | Approve this plan and the DS subset (D1–D9) | DS start |

## 4. Order

1. O6 → DS runs D1 → D2 → D3 → D4 → D5 → D6 → D7 → D8 → D9 (one task per
   report, stop on any failure). Strong-model review of each DS diff before the
   next task (cheap: diffs are small).
2. In parallel, strong model: S1 (highest value), then S2.
3. After O4: S3, then S4.
4. S5 after S2 (needs current SC state), S6 after D9, S7 any time.

## 5. Acceptance for the whole plan

- `start --json` shows header ABI 15, a `freshness` block flagging the stale
  checkpoint, and recent entries; no checkpoint content changed by tooling.
- `inspect 0x4C6E80` resolves and links `f1c_vehicle_native_clear.h` and
  `tests/test_vehicle_native_clear.py` without changing trust labels.
- `search` median < 2 s on the three benchmark queries in the handoff, with
  results identical to the fallback path.
- `sdk-test --no-regenerate` writes nothing; default `sdk-test` reports which
  generated files changed and preserves their before-copies.
- `evidence-query findings replay` returns September checkpoint records labelled
  `RECORDED`.
- Every edited file has a before-copy under
  `workspace_organization_audit/LLM_TOOLING_IMPL_BEFORE_<date>/`.
- Workspace validator shows no new errors (baseline 11).

## 6. Explicit non-goals

No broad IDA/prose scrape, no directory moves, no production/game/IDB writes by
DS, no trust-tier promotion by tooling, no checkpoint objective edits without O5,
no commits without O2.

## 7. Pending

All tasks unstarted. No speedup is claimed until D6 benchmark is recorded.

## Owner decision log

- 2026-09-30: O6 approved. DeepSeek assigned D1–D9 in **run-straight-through**
  mode (no per-task review stop). Stop conditions in handoff section 3 still
  apply. Strong-model review happens once, after D9, from
  `LLM_TOOLING_IMPLEMENTATION_LOG.md` and `git diff`.
- 2026-09-30: Follow-up batch E1–E4 (drift timestamp, doc-link noise, search
  records, findings JSON) prepared in
  [DEEPSEEK_HANDOFF_2_2026-09-30.md](DEEPSEEK_HANDOFF_2_2026-09-30.md);
  run-straight-through mode.
- 2026-09-30: Follow-up batch G1–G3 (safe digest decode, banner-free
  evidence-query, search fallback notice) prepared in
  [DEEPSEEK_HANDOFF_3_2026-09-30.md](DEEPSEEK_HANDOFF_3_2026-09-30.md).
- 2026-09-30: address harvest handoff H1–H3 prepared in [DEEPSEEK_HANDOFF_4_ADDRESS_HARVEST_2026-09-30.md](DEEPSEEK_HANDOFF_4_ADDRESS_HARVEST_2026-09-30.md). The index held about 160 proven entries while the SDK headers used 72 offsets absent from it.
- 2026-09-30: tier-proposal handoff T1–T2 prepared in [DEEPSEEK_HANDOFF_5_TIER_PROPOSALS_2026-09-30.md](DEEPSEEK_HANDOFF_5_TIER_PROPOSALS_2026-09-30.md). Harvest reviewed by Claude: 464 entries, tests pass, and the conflicts are mostly synonyms.
- 2026-09-30: address merge reviewed (13 vehicle fields + 17 globals). Next handoffs: [6 live-memory named fields](DEEPSEEK_HANDOFF_6_MEMORY_FIELDS_2026-09-30.md) and [7 evidence for unproven addresses](DEEPSEEK_HANDOFF_7_ADDRESS_EVIDENCE_2026-09-30.md); they are independent and can run in either order.
