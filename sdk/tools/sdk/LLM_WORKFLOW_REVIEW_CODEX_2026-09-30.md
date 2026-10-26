# LLM workflow, tooling and SDK review — Codex, 2026-09-30

## Scope and conclusion

Owner request: assess the project directory/workflow, purpose-built tool quality,
SDK quality and marginal value of further static analysis/IDA extraction for LLM use.
Product: `f1c-sdk`; canonical directory: `f1c_ida_decompiled`. The product registry
records implement/candidate, next gate verify. This task is a review/investigation,
not a change to that product stage. Documentation destination is this report,
indexed from the canonical SDK README. No runtime changes or implementation requested.

Acceptance: inspect actual CLI behavior and representative implementations/tests;
identify concrete avoidable effort; distinguish evidence, inference and proposed work.
Non-goals: reorganization, live-game operations, deployment, SDK regeneration,
whole-codebase certification or exhaustive reverse engineering.

**Assessment (inference):** strong engineering foundations, uneven discoverability
and current-state maintenance. The largest likely gains are reconciling current
truth, retrieving recent evidence, and repeatable lifecycle validation. Broad new
decompiler scraping is a lower priority. No performance improvement was measured.

Evidence labels below: Observed = this session's commands/files; Source-verified =
implementation inspection, not execution proof; Recorded = existing documentation;
Proposal = future work, not implemented. This is a sampled review.

## Observed findings and recommendations

### 1. Current-state reporting contradicts implementation

Observed: `python tools/f1c_mod.py sdk-info --json` reports ABI 11.
Source-verified: `f1c_broker_abi.h:7` defines ABI 15. `cmd_sdk_info` reads the
checkpoint's ABI field. `AUTOMATION_CHECKPOINT.json` has updated_at 2026-08-24
and broker_abi_version 11 despite newer September evidence entries. Startup
priorities still refer to ABI-11 field telemetry and earlier ESC validation.
Some broader acceptance can remain pending even after a release; do not mark
it passed merely because the product shipped. The current summary needs reconciliation.

Recorded: the SDK README's base description says packaged ABI 7, with later
versions scattered through additions. The workspace product registry lists ESC
v1.0.3 shipped, while development/mods/README.md contains older product positions.

Observed: `doctor` passes and verifies the canonical EXE. Source-verified: it checks
required files, catalogue presence, tool classifications and runners, and optional
EXE identity, but does not reconcile ABI/current status or run the full test gate.
The validate-sdk tool description overstates this by mentioning the complete gate.

Proposal: generate a compact current view from maintained capability/product
records. Keep historical entries intact. Add consistency checks for header,
checkpoint and generated reference; represent installed/verified/shipped versions
separately. Report stale/conflicting/unknown explicitly. Maintain human-validation
scope and exact artifact identity rather than inferring completion.

### 2. The corpus is extensive; recent knowledge is not consistently retrievable

Observed `evidence-query stats`: 7,138 functions with facts, 4,505 fills, 572
findings, 7,895 globals, 775 config fields across 36 parsers, and 4,656 runtime
vehicle fields. These are different stores and confidence levels, not public APIs
or a measure of complete EXE semantics.

Observed SDK catalogue: 6,383 function entries: **47 CONFIRMED, 6,336 LEGACY**;
113 vehicle fields, 165 legacy structs, 181 vtables. Handwritten helpers also
exist outside these entries. The 47 count is not every usable SDK operation,
and a reviewed ABI is not automatically a safe standalone command.

Concrete retrieval experiment:

- `sdk-query function replay --limit 3 --json` returns three LEGACY entries.
- `inspect 0x004C6E80 --max-lines 8 --json` returns raw/fact/derived sources and
  the LEGACY catalogue name ResetDriverGridState.
- The SDK has f1c_vehicle_native_clear.h, a focused test and a September README
  evidence link. Those are absent from this inspection response. The helper's
  restricted contract must remain separate from the raw function's trust tier.
- `search f1c_vehicle_native_clear --limit 3 --context 0 --json` returns no source
  matches. This is consistent with the command's decompiler-only scope, not proof
  that the helper is absent.
- `evidence-query findings replay` returns five records but not the newer September
  replay ownership records seen in the checkpoint/README. Older open failures
  must not be treated as disproven merely because newer related work exists.

Source-verified: source_explorer.py searches four corpus directories; ask.py joins
established JSON stores and findings.jsonl; sdk-query scans function/vehicle-field
entries. Current headers and development-product evidence are not automatically
joined into these answers.

Proposal: extend the existing front door with an index joining symbols, fields,
semantic operations, current helpers, tests, product verification, failed attempts
and superseding corrections. Preserve target identity, provenance, trust tier,
scope and pending proof. A good answer should return the supported route, known
failed approaches and next missing evidence. Avoid another competing truth store.

### 3. Retrieval has avoidable scanning and output cost

Source-verified: search_sources enumerates and reads the selected corpora for every
query, then sorts and applies --limit. Small result limits do not bound I/O.
No precise latency benchmark was taken in this session. The no-match search ran
asynchronously; no numerical speedup is claimed.

Observed: inspect --max-lines 8 still returned a large node_lookup with neighbor
metadata and offset arrays; the limit bounds source snippets, not total output.
Source-verified: sdk-query truncates matches before reporting count and lacks a
separate total count/cursor. evidence-query has no JSON flag and emits a subprocess
banner. recommend uses keyword scoring; a request about verified replay/lifecycle
APIs ranked memory-mcp health first, a possible lead rather than a semantic plan.

Proposal: incremental indexed search behind the same CLI, with content identities
and stale-index detection/fallback; compact/default and expanded views; graph
depth/output budgets; total/returned counts; structured no-match/error envelopes.
Benchmark real retrieval tasks before introducing embeddings or new graph tools;
the repository already has call graphs and evidence joins.

### 4. SDK foundations are sound, but surfaces offer different guarantees

Source-verified strengths: broker capability negotiation, pinned discovery,
duplicate-broker refusal, generation/object checks, reviewed data widths,
game-thread contracts and native fingerprints. The race-penalty helper distinguishes
native queue insertion from later scoring acceptance and exposes an uncertain
result that must not retry. These are valuable LLM-facing protections.

The generated f1c_reviewed_api.hpp wrappers directly call addresses. A namespace
and documented thread requirement do not enforce every runtime precondition.
Broker APIs, guarded helpers and ABI-only wrappers need visibly different guarantees.

Proposal: machine-readable records for observation, guarded semantic commands
and expert/native primitives. Include code/layout compatibility, Lightloader host,
thread, lifetime, ownership, preconditions, acknowledgment/completion, failure and
recovery, tests, and exact native-acceptance evidence. Prefer promoting a few
repeatedly used product primitives over wrapping hundreds of additional addresses.

Compatibility scope matters: development/AGENTS.md's 2026-09-24 owner standard
requires first-party mod host proof and required code/layout checks rather than
whole-file EXE hashes. Research/tests/memory-reader paths use exact canonical
identity for different purposes. Document the distinction; do not globally relax
identity checks or claim all mods have migrated.

### 5. Tests have substance; orchestration mixes refresh and verification

Source-verified: test_broker_penalty_hold.py compiles the actual broker implementation
with isolated vehicles and checks stale tokens/sessions/objects, thread, queue and
ownership cases. Replay fixtures execute captured native bytes with external
services stubbed and explicitly disclaim physics acceptance. Variant tests exercise
synthetic relocation and wrong-resolution cases. These are meaningful checks,
although source-text checks also exist.

Source-verified: sdk-test defaults to run_all.bat, which first regenerates five
SDK/index outputs. It is not a read-only verification command. It repeats session
guard, safety-car guardrails and vehicle lifecycle tests. It prints 'All canonical
SDK tests passed.' before later tests run. Later failures still propagate with
exit /b 1, so this is misleading intermediate output, not a demonstrated false
final exit code. Public suite options are all, qualifying-order and garage-results.

Proposal: separate explicit refresh from check-generated consistency, preserve
outputs, use dependency-aware focused suites, and emit one final gate result.
Retain complete gates for shared ABI/lifecycle work and wherever current policy
requires them. A policy change to focused-only validation needs its own reviewed
scope; this review does not silently relax requirements.

### 6. Runtime lifecycle validation is likely the next high-value investment

Recorded documentation repeatedly leaves native replay, exit/reentry, repeated
restarts, cross-mod behavior and presentation pending after deterministic passes.
The SDK README records a local-yellow loading freeze after deterministic tests
passed. Static knowledge alone does not establish whole-session behavior.

The memory MCP design fits LLM observations: bounded fixed reads, read-only process
rights, local IPC, process identity, capture time and advancing/unchanged freshness.
The guide identifies snapshot atomicity and attribution limits. No f1c-memory
connector was exposed among this turn's callable tools; the CLI fallback exists.
No reader was started and no live sample was taken; runtime health is unverified.

Proposal: extend existing capture/regression tooling with reusable scenarios for
replay with retirees, exit/reentry, repeated SC/RF, unload and slot reuse. Record
mod/broker/config identities, transition sequence, expected invariants and failure
trace. Replay observations offline for policy regression while preserving native
and human acceptance as separate evidence. Prefer event-linked capture over
asking an LLM to poll unchanged state.

### 7. Directory governance should be reconciled before further reorganization

Observed validator before and after the README index edit: exit 1, same **11 errors,
0 warnings**. Unexpected top-level .pytest_cache, f1 challenge testing versions,
trials; missing F1 Challenge V10_clean counted as both required and canonical path;
six source-like production Python files (four bug-report-site files across v1.0.0/
v1.0.1 and two telemetry-service files). These are contract failures, not proof that
Python service payloads cannot be legitimate releases or permission to relocate them.

The root-linked research/game_mechanics/README.md and AGENTS.md are absent at those
paths; the research directory listing lacks game_mechanics. No replacement location
was investigated. Absence at the documented path does not establish deletion/loss.

Proposal: keep the top-level layout; resolve discrepancies through owner-reviewed
contract/path dispositions. Preserve historical/audit records. Shorten active entry
points into current routing/authority views with linked history, under a scoped
governance change. The existing mandatory cold start remains in force.

## When more IDA/static work is useful

Inference: another broad pseudocode/prose-fill pass is unlikely to be the best next
investment. Existing stores are substantial, but do not prove complete indirect-call
coverage, semantics or command safety. Targeted work remains valuable for a selected
feature's unresolved native writer/owner/lifetime, exact ABI or access width,
indirect-call target, executable-variant invariant, or renderer/UI hook.

First identify the implementation decision the answer would change and query the
existing exports. If evidence is missing, extract only the required typed accesses,
xrefs/callsites, control-flow slice and types, with target/function-byte identity
and database provenance. Do not duplicate existing graphs without a quality gap.
Use the supported ida-prepare route for isolated work. No IDA availability/export
coverage checks were performed here; gameplay remains a separate proof.

## Prioritized next work and acceptance

1. Reconcile current truth: header/checkpoint/tool views agree and stale evidence
   is explicit; root path/validator issues have dispositions. No invented passes.
2. Unified retrieval: real questions find the current helper, evidence, failed
   approaches and pending proof without manual filename discovery. Use the
   native-clear and replay examples above as regression cases.
3. Separate refresh/check: verification does not silently rewrite inputs; focused
   suites identify coverage and complete final gates remain policy-compliant.
4. Capability records plus reusable lifecycle scenarios: promote a small set of
   frequently used operations with independent tests and native evidence.
5. Targeted RE only for the next unresolved dependency; preserve negative results.

Measure commands to correct evidence, time to first justified change, repeated
rediscovery, test duration and regressions caught. No baseline/speedup was measured.

## Verification, changed files and restoration

Executed CLI smoke checks: start, sdk-info, doctor, route, capabilities, recommend,
SDK stats/query, evidence stats/query, source search and inspect. Source-reviewed
representative helpers and tests. Git showed 47 tracked modified paths before
documentation; existing work was preserved. No runtime/source/production edits,
builds, refresh, game writes, IDA actions or deployment were performed.

Full sdk-test was not run: its regeneration/build side effects were unnecessary
for this review. No fresh suite pass or new gameplay validation is claimed.

Documentation concurrency: a different review was found at the initially chosen
LLM_WORKFLOW_REVIEW_2026-09-30.md path during verification. It contains measurements
not performed in this session. Its author/timing was not established. It is left
untouched; this distinct CODEX filename separates the evidence. Do not merge its
claimed timing/full-gate results into this session's verification.

Final intended documentation changes: this report and the SDK README review link.
Before-state directory (workspace-relative):
workspace_organization_audit/LLM_WORKFLOW_REVIEW_BEFORE_2026-09-30/f1c_ida_decompiled/tools/sdk/.
README.md there preserves the full original SDK README with verified matching SHA256
25854052B07E0769F8B148C521107835855268FF69E96CEC817CB9CD44F7E261.
README.after-first-index.md preserves the intermediate README before retargeting
the review link. Both map to f1c_ida_decompiled/tools/sdk/README.md. Restoration:
preserve the then-current file and reconcile this task's additive link against
the original backup; retain both reports and audit evidence. No deletions/moves.
No global product-status or capability-completion change is justified by this review.

Documentation gate: final record/index verification and final validator result
are appended after saving the corrected index link. Pending work is all proposed
implementation, measured performance and native/human acceptance.

## Wasted-effort check

- Repeated large reads: initial combined output exceeded its budget; later reads
  were narrowed. Some inventories were more verbose than necessary.
- Broad builds/tests: none; checked the full gate's side effects before declining
  to regenerate/build during this documentation review.
- Avoidable failures/retries: absent root-linked research docs, one incorrect test
  directory assumption and one Windows wildcard rg invocation. Corrected with
  scoped paths. The negative search is retained as retrieval-scope evidence.
- Polling/waiting: bounded command completion checks only; no live-game polling.

## Final documentation gate

Verified the distinct Codex report exists, its SDK README link resolves, and it distinguishes observed/source/recorded evidence from proposals and names pending validation. Final workspace validator after correcting the link: exit 1, unchanged 11 baseline errors, 0 warnings. No new validator error was observed. The separate concurrent review remains preserved. No full SDK suite or live gameplay was run by this session.
