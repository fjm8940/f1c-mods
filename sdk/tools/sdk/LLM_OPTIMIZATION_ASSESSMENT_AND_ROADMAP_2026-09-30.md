# F1 Challenge: LLM Workflow, Tooling and SDK Assessment

**Date:** 2026-09-30  
**Status:** Draft assessment and proposed roadmap; implementation not started  
**Perspective:** An LLM working repeatedly in the existing F1 workspace  
**Canonical component:** `f1c_ida_decompiled` / F1 Challenge Mod SDK  
**Product stage:** `implement`, maturity `candidate`, next gate `verify`  
**Evidence record:** [Codex review and verification limits](LLM_WORKFLOW_REVIEW_CODEX_2026-09-30.md)

## 1. Executive assessment

The project already possesses substantial knowledge of the game and a capable set of purpose-built tools. The next improvement should be to make that knowledge easier to retrieve, reconcile and use through dependable SDK capabilities.

The strongest foundations are already present: one supported command-line entry point, evidence tiers, canonical development locations, a shared broker, capability negotiation, lifecycle checks, guarded native helpers, and meaningful regression fixtures. These are valuable assets to retain.

The main weakness is the distance between what the project knows and what a new LLM session can reliably discover. Recent implementation and product evidence are distributed across headers, tests, trial records, README additions and checkpoint entries. The principal retrieval tools do not consistently join those sources. Current-state summaries also disagree with implementation in places.

That combination creates repeated work. An LLM may find an old name or an unresolved historical finding, miss a newer guarded helper, reopen a question that has already been investigated, and spend time reconstructing the project's current state before making a useful change.

The recommended order of investment is:

1. Reconcile current status and capability information.
2. Connect existing retrieval to current implementation and evidence.
3. Reduce search, output and verification overhead.
4. Make SDK guarantees explicit and promote frequently used semantic operations.
5. Build repeatable native lifecycle scenarios around existing tooling.
6. Perform additional static analysis when a specific unanswered question blocks one of those efforts.

This is a qualitative assessment. It does not claim a measured speedup, a percentage of SDK completion, or complete knowledge of the executable.

## 2. Scope, evidence and authority

### 2.1 Questions addressed

This document develops the owner's request to review the directory and workflow, judge custom-tool and SDK quality, and assess whether further static analysis or IDA extraction would improve LLM-driven development.

It covers the SDK and the surrounding workflow where they affect LLM productivity. It is not an exhaustive audit of every mod, every native call, every test, or every reverse-engineering claim.

### 2.2 Evidence categories

| Label | Meaning |
|---|---|
| Observed | A command response or filesystem state obtained in the preceding review |
| Source-verified | Behavior established by reading the implementation; not a new runtime test |
| Recorded | An existing project record reports the result; it was not rerun for this assessment |
| Inference | A conclusion drawn from those observations and records |
| Proposal | Recommended future work, not an existing capability or approved implementation |

The corpus counts and retrieval examples below belong to the September 30 review. They are a dated baseline, not permanent properties of the project. The drafting follow-up reran orientation and the workspace validator but did not repeat the full investigation.

### 2.3 Boundaries

Existing workspace and SDK instructions remain authoritative. This proposal does not authorize structural moves, replacement of preserved evidence, game writes, changes to production, or reduced validation requirements. It does not change any product's lifecycle stage.

Historical records should retain their original claims and paths. Corrections should identify what changed and why; retrieval should expose that relationship. A newer note must not automatically erase an older failure or imply that all related behavior is now correct.

## 3. What makes this workspace efficient for an LLM

An effective LLM workflow minimizes the work needed to answer five questions:

1. What is the current authorized objective and canonical location?
2. What does the project already know about this behavior?
3. Which existing interface is appropriate, and what does it guarantee?
4. What is the smallest justified change and its relevant verification?
5. What remains unknown, including human validation?

More context is useful only when it improves those answers. Unbounded source dumps, repeated historical summaries and contradictory status increase the chance of choosing the wrong implementation route.

The ideal progression is a compact current briefing, a bounded evidence dossier, the relevant interface contract, focused implementation, and explicit verification. The LLM should be able to expand into raw code or historical records when necessary without having to read them all first.

This progression must preserve the distinction between knowledge and authority. Knowing an address, observing a field, reviewing an ABI and approving a semantic command are different accomplishments.

## 4. Overall quality assessment

| Area | Assessment | Main reason | Highest-value improvement |
|---|---|---|---|
| Directory architecture | Sensible boundaries, with map/contract drift | Development, testing, production and operations have established roles | Reconcile actual paths and validator dispositions |
| Tool entry point | Strong foundation | One supported CLI routes work and classifies tools | Keep it, while improving consistency and coverage |
| Evidence retrieval | Useful but incomplete | Established stores are joined; recent product evidence is not consistently included | Connect helpers, tests, records and corrections |
| Tool ergonomics | Uneven | Some structured output is compact; other responses and searches are expensive | Output budgets, pagination and incremental indexing |
| SDK architecture | Good foundations | Broker ownership, negotiation, lifecycle and native guards | Make guarantees visible by capability |
| SDK maturity | Varies by operation | Reviewed APIs, expert wrappers and experimental helpers coexist | Separate maturity and safety dimensions |
| Automated verification | Substantive in sampled areas | Actual implementation and native-byte fixtures are exercised | Separate refresh from checks; improve suite selection |
| Native acceptance | Still a recurring constraint | Lifecycle and presentation limits remain explicit in records | Reusable scenarios and event-linked evidence |
| Broad additional static work | Lower priority | Existing corpus is extensive and retrieval gaps are demonstrated | Investigate only a defined unresolved dependency |

These judgments concern the inspected areas. A good broker architecture is not evidence that every consumer uses it correctly, and a strong fixture does not certify all gameplay paths.

## 5. Directory and documentation workflow

### 5.1 Keep the established layout

The existing separation of development, testing, immutable releases, operations and publishing is useful. Another broad directory reorganization would introduce restoration and dependency work without directly addressing the main retrieval problems observed here.

The SDK's stable root is also useful. An LLM benefits from one canonical location for public headers, contracts, tooling and evidence. Historical copies can remain preserved and classified without competing as active development locations.

The proposed improvement is to make the map accurate and the current entry points concise. Physical proximity alone will not connect a native function to the product record that explains its latest restrictions.

### 5.2 Resolve the validator's repeated failures

The review and drafting orientation both observed 11 workspace-validator errors, with zero warnings:

| Category | Reported entries |
|---|---:|
| Unexpected top-level `.pytest_cache`, `f1 challenge testing versions`, `trials` | 3 |
| Missing `F1 Challenge V10_clean`, reported as both a required directory and canonical path | 2 |
| Source-like Python files in production bug-report-site releases and the telemetry-service release | 6 |

These are contract failures, not automatic instructions to move files. In particular, a deployed Python service can legitimately contain Python source. The appropriate production rule depends on the approved artifact type and release contract.

Each finding needs a disposition: actual defect, approved exception, outdated contract, or unresolved owner decision. A future validator should distinguish those categories and retain the evidence for accepted exceptions. It should continue to reject genuinely unapproved structure.

The review also found that root-linked `research/game_mechanics/README.md` and `AGENTS.md` were absent at their documented paths. No replacement location was investigated. The finding establishes a broken map, not deletion or loss of the research.

### 5.3 Separate current state from the historical sequence

The current documentation pattern often adds dated developments before or around an older description. This preserves useful history, but requires a reader to determine which statements supersede which others.

The proposed information model has three distinct views:

| View | Purpose |
|---|---|
| Current state | Present implementation, relevant versions, current objective, unresolved gates |
| Evidence | Exact tests, artifacts, observations, failures and owner results supporting that state |
| History | Prior approaches and transitions, retained for explanation and restoration |

These views can point to existing files. They do not require moving historical material or adding another mandatory reporting layer to every small task.

### 5.4 Generate a compact briefing from maintained records

A future startup briefing should include product identity, canonical path, applicable instructions, current objective, present capability versions, relevant evidence and the next unresolved gate. It should indicate conflicts instead of silently selecting one statement.

It should distinguish source state, built candidate, installed artifact, verified artifact and public release. Those identities can differ legitimately. A shipped artifact may have an explicitly limited acceptance scope.

Any reduction of the current mandatory cold-start reads requires a reviewed instruction change. Until then, the existing bootstrap remains required. The briefing is a proposed improvement to that process, not permission to bypass it.

## 6. Custom tools: strengths and concrete gaps

### 6.1 Retain the single CLI entry point

`python tools/f1c_mod.py` provides a consistent entry to routing, discovery, evidence, SDK queries, mod workflows and runtime observations. Its tool catalogue expresses purpose, effects and selection priority. That reduces the chance that an LLM selects a historical script simply because a filename looks relevant.

The next improvements should be added behind that interface. A second public collection of scripts would make selection and maintenance harder unless it had a clearly separate, approved purpose.

### 6.2 Make reported state agree with code

Observed in the review: `sdk-info --json` reports broker ABI 11 while `f1c_broker_abi.h` defines ABI 15. Source inspection shows that the CLI reads the checkpoint field. That checkpoint retains an August 24 main timestamp and older objectives alongside September entries.

The SDK README also contains an older packaged ABI-7 description with later revisions in dated additions. These inconsistencies are especially costly in the cold-start path because an LLM is told to treat it as the current briefing.

`doctor` passed despite the disagreement. Its implementation checks required files, catalogue presence, tool classifications, runners and optional executable identity. It does not reconcile current ABI/status or execute the complete gate. The tool description should accurately describe that scope.

Proposed acceptance: the status command either agrees with the canonical interface or returns a structured conflict. Doctor results should list which checks ran and which did not. A green prerequisite check should never imply native acceptance or a full test run.

### 6.3 Join research and recent product evidence

The review reproduced a useful example around `0x004C6E80`:

- Inspection returned raw/fact/derived sources and the older LEGACY name `ResetDriverGridState`.
- The SDK already contained a native-clear helper, a focused test and a September evidence link.
- Those current connections were absent from the inspection response.
- Searching for the helper through source search returned no matches because source search covers decompiler corpora, not current SDK headers.

The search result is consistent with the command's current scope. The missing capability is a cross-source answer to the practical question: what is the supported way to perform or investigate this operation today?

A proposed evidence dossier should join the raw symbol to current helpers, restrictions, tests, product consumers, failed attempts, corrections and pending acceptance. It must avoid upgrading the raw function's trust merely because a narrow guarded helper exists.

### 6.4 Improve search execution without changing evidence semantics

Source inspection shows that each search enumerates and reads the selected corpora, then sorts and applies the result limit. A small result count does not avoid scanning all selected files.

An incremental text index is a reasonable candidate. It should track source identity, retain provenance, detect changed files and explicitly handle stale data. An index miss should be distinguishable from an authoritative absence.

The choice of storage engine should follow a small benchmark and operational needs. The review did not compare alternatives or measure a proposed speedup. Introducing embeddings before fixing corpus coverage would leave the main gap unresolved.

### 6.5 Make output predictably bounded

`inspect --max-lines 8` still returned a substantial neighboring graph structure. The source excerpt limit does not constrain the whole response. `sdk-query` truncates results before reporting its count, and the inspected path does not expose a separate total-match count or cursor. `evidence-query` does not offer JSON output and includes a subprocess banner.

A proposed common response contract should contain:

| Field | Purpose |
|---|---|
| Schema and operation | Stable parsing and versioned behavior |
| Status | Success, no match, partial result, conflict or failure |
| Query and scope | Explain what was actually searched |
| Total and returned counts | Make truncation visible |
| Continuation or expansion hint | Retrieve more evidence deliberately |
| Sources and evidence tiers | Preserve provenance and confidence |
| Freshness | Identify source/index versions and staleness |
| Side effects | Distinguish observation, artifact writes and live actions |

The default response should answer the immediate question. Raw bodies and neighboring graphs should be deliberate expansions. This is a proposed contract; it is not the current CLI schema.

### 6.6 Treat recommendations as routing assistance

The current recommendation implementation uses keywords. In the sampled request about verified replay/lifecycle APIs, it ranked memory health first. That can be useful context, but does not solve the research question.

Tool selection should make effects and relevance clear. When several routes apply, a compact sequence with reasons is more useful than an unqualified top match. A recommendation remains advice; it should not automatically execute a live action.

## 7. SDK quality and the next maturity step

### 7.1 What is already good

Sampled SDK code demonstrates broker discovery and capability negotiation, module lifetime handling, duplicate-broker refusal, generation and object checks, reviewed widths, native fingerprints and game-thread contracts.

The race-penalty helper makes a particularly useful distinction: an event entering the native queue does not yet establish that scoring accepted the penalty. It also exposes an uncertain result that must not be blindly retried. Such behavior prevents an LLM from confusing request submission with completion.

These patterns should become consistent across the public surface. They reduce the amount of hidden game knowledge a consumer must carry.

### 7.2 Catalogue size is not SDK maturity

The reviewed catalogue contained 6,383 function entries: 47 CONFIRMED and 6,336 LEGACY. It also contained 113 vehicle fields, 165 legacy structures and 181 vtables.

Those numbers have limited meaning outside their schemas. Handwritten helpers exist beyond the reviewed-function entries. A confirmed calling convention does not prove that a function is safe as a standalone operation. Conversely, a small set of carefully designed operations can support many useful mods.

A better maturity question is whether a selected operation has a clear contract, reliable refusal behavior, representative tests, current evidence and an example that follows the supported lifecycle.

### 7.3 Distinguish three interface classes

| Interface class | Expected guarantee | Typical remaining responsibility |
|---|---|---|
| Observation | Read a defined state with identity, validity and freshness information | Interpret the result within its evidence scope |
| Guarded semantic command | Request a reviewed behavior through owned boundaries with explicit preconditions and results | Handle refusal, completion and declared recovery rules |
| Expert/native primitive | Expose a reviewed ABI or layout with narrower guarantees | Establish target, lifetime, thread and operation-level safety |

Generated wrappers in `f1c_reviewed_api.hpp` directly invoke addresses. Their namespace and documented thread requirement are useful, but do not enforce every runtime precondition. They should not look equivalent to an operation that performs all relevant checks.

The distinction should be discoverable in code, documentation, tooling and examples. Existing names need not be changed immediately if metadata and guidance can express it accurately.

### 7.4 Define a capability record

For each selected public operation, maintain one record with:

- Stable identity, purpose and owner.
- Interface class and maturity status.
- Required host, broker ABI/capabilities and code/layout compatibility.
- Valid thread, session phases, object identity and lifetime.
- Inputs, observations, side effects and ownership constraints.
- Refusal, acknowledgment, completion and uncertain-result semantics.
- Retry rules, restoration/recovery and unload behavior.
- Static/ABI evidence, deterministic tests and native acceptance scope.
- Known incompatibilities, negative evidence and pending validation.
- Supported example and product consumers.

Keep evidence tier separate from interface class and maturity. For example, a native primitive can have a well-reviewed ABI while still being unsuitable as a general command. One successful gameplay scenario does not establish every lifecycle guarantee.

### 7.5 Promote useful operations rather than more addresses

Select operations already repeated across products or repeatedly rediscovered by agents. For each, identify common semantics, preserve product-specific policy in the consumer, and place shared validation and ownership in the appropriate SDK/broker layer.

Potential categories include session observations, generation-safe vehicle access, race-control coordination, and request/completion handling. Selection must follow actual consumer needs and evidence; this document does not certify a particular helper for promotion.

### 7.6 Keep compatibility rules scoped

The owner standard recorded in development instructions requires Lightloader host proof and required code/layout checks for first-party mods, rather than whole-file EXE hashes as the compatibility gate. Research fixtures and exact-target memory readers use canonical identity for different purposes.

Document those scopes explicitly. Package-integrity hashes, test-input identity and runtime compatibility are separate concerns. Do not remove exact-input checks from research merely to make terminology uniform, and do not assume every existing mod already satisfies the newer host protocol standard.

## 8. Verification and native evidence

### 8.1 Recognize the existing test quality

The inspected broker penalty tests compile actual implementation against isolated vehicles and exercise stale tokens, sessions, objects, threads, queue limits and ownership. Replay fixtures execute captured native bytes with external services stubbed. Variant fixtures exercise synthetic relocation.

These tests provide meaningful evidence. Their explicit limitations are also valuable: stubbed scene services do not establish real vehicle physics, and synthetic relocation does not certify every executable variant.

The review did not rerun the full suite. Statements here concern source inspection and recorded evidence, not a fresh certification.

### 8.2 Separate regeneration and checking

The current full `sdk-test` path starts by regenerating five SDK/index outputs. It also repeats some suites and prints a success message before later checks. Later failures still propagate; the problem is mixed responsibilities and potentially misleading intermediate output.

A proposed verification design separates:

1. Prerequisite and consistency checks.
2. Explicit generation into preserved/versioned output.
3. Comparison of generated output with the expected source-derived state.
4. Capability-focused deterministic suites.
5. Complete integration gates where required.
6. Native scenarios and human acceptance.

A focused suite must state exactly what it covers. It cannot silently replace the currently mandated full gate. Shared ABI or lifecycle changes deserve broad regression coverage. Any change to gate policy needs separate review and documentation.

### 8.3 Make scenarios reusable

Runtime transition problems recur in the records: replay, returning to a session, repeated restarts, retirement, object reuse and interactions between mods. The SDK README also records a loading freeze after deterministic tests passed.

A proposed reusable scenario contains the initial state, approved actions, expected transitions, invariants, capture triggers and acceptance conditions. Use existing capture and guard tooling where possible.

| Scenario family | Evidence to collect | Example question |
|---|---|---|
| Session exit and reentry | Session generations, identities, callbacks and cleanup results | Does stale state survive into the next race? |
| Replay with retired vehicles | Roster identity, visibility, ownership and before/after state | Does playback alter the active field incorrectly? |
| Repeated SC/RF operation | Ownership, handoff, command and completion events | Does a later cycle retain state from the earlier cycle? |
| Unload and reload | In-flight callbacks, handles and resource ownership | Is an unloaded client still reachable? |
| Slot/object reuse | Old/new generation and vehicle identities | Is a reused slot mistaken for the former object? |
| Presentation | Recorded HUD/audio context and owner feedback | Does a mechanically correct result communicate correctly? |

These are proposed scenario families, not claims of existing automation or completed coverage.

### 8.4 Attach exact identities to evidence

A useful run record includes executable/test identity, broker and mod hashes, relevant configuration identity, scenario version, capture times, observed progression, assertions, logs and human result. It should identify missing evidence explicitly.

Recorded observations can be replayed offline to exercise mod policy. That tests behavior against the captured sequence; it does not replay the whole game or prove native side effects. Keep those claims separate.

### 8.5 Prefer event-linked observation

The memory MCP's bounded read-only observations, process identity and freshness handling are well suited to LLM diagnostics. The guide correctly warns that unchanged samples do not prove advancing gameplay and that external reads are not fully atomic.

For longer investigations, prefer bounded capture around relevant events or transitions over repeated conversational polling. Report absence of a transition as a result, with a time window and explicit limits. Runtime-reader availability was not tested in this drafting task.

## 9. Should there be more static analysis or IDA scraping?

### 9.1 Broad extraction has diminishing value at this stage

The earlier inventory reported 7,138 functions with facts, 4,505 fills, 572 findings, thousands of globals and hundreds of parsed configuration fields. Caller/callee and other knowledge stores already exist.

This does not prove that all semantics are understood. It does show that the workspace has substantial discovery material while demonstrated retrieval and capability gaps remain. Another broad pseudocode dump or prose-fill pass would not directly fix those gaps.

### 9.2 Targeted static work remains useful

Additional analysis is justified when it answers a selected implementation question:

| Uncertainty | Useful static evidence | Separate proof still needed |
|---|---|---|
| Native ownership/lifetime | Constructor, writer, teardown and caller relationships | Actual transitions and resource behavior |
| Calling convention or layout | Callsite, stack cleanup, width and pointer-adjustment evidence | Guarded integration and relevant native execution |
| Indirect dispatch | Vtable/callsite targets and control flow | Which path occurs in the selected scenario |
| Variant compatibility | Required code patterns, layout invariants and refusal cases | Approved target matrix |
| UI/render integration | Defined update/draw/input boundaries and dependencies | Presentation, timing and interaction acceptance |
| State field interpretation | Writers, consumers, conditions and transformations | Natural behavior or a separately authorized causal experiment |

### 9.3 Apply a stopping rule

Before starting a new analysis pass, record the question, existing evidence, missing information and implementation decision it will affect. Define the smallest extraction that can answer it.

Stop when the decision is sufficiently supported, when evidence disproves the route, or when the remaining uncertainty requires runtime observation. Preserve the negative result. Do not continue expanding the call graph solely because additional functions are available.

### 9.4 Prefer structured, attributable exports

Where exports are missing, useful output may include typed accesses, exact xrefs/callsites, a bounded control-flow slice, relevant types and function/target identity. Preserve raw evidence alongside interpretation and record database provenance.

First check existing exports. Do not duplicate whole graphs unless a specific accuracy, freshness or completeness problem justifies regeneration. Use the supported isolated IDA workflow. This assessment did not test IDA availability or compare export formats.

## 10. Proposed implementation roadmap

Relative effort below is a planning judgment, not an estimate in days. Each package requires its own definition/specification before implementation.

| Order | Work package | Expected value | Relative effort | Dependency |
|---|---|---|---|---|
| 1 | Current-state consistency | Fewer wrong starts and stale assumptions | Small to medium | Confirm authoritative records |
| 2 | Unified evidence retrieval | Less rediscovery and missed prior work | Medium | Explicit source/provenance rules |
| 3 | Output/search efficiency | Lower interaction and I/O overhead | Medium | Representative query baseline |
| 4 | Verification separation | Clearer results and fewer unnecessary writes/runs | Medium | Existing gate policy and suite inventory |
| 5 | Capability contracts | Safer reuse with less implicit knowledge | Medium, incremental | Select real consumer needs |
| 6 | Lifecycle scenario library | Better regression evidence for recurring failures | Medium to large | Stable observations and scenario identities |
| 7 | Targeted RE | Resolve the next actual blocker | Bounded per question | Explicit unanswered dependency |

### 10.1 Package 1: current-state consistency

Deliver a compact status view, reconciliation checks and explicit dispositions for stale or conflicting records. Preserve historical before-states. Keep installed, verified and released identities separate.

Acceptance: ABI disagreement is detected; stale objectives are visible; current paths resolve or carry an explicit unresolved disposition; pending human validation remains pending; doctor describes its actual coverage.

Abort or defer if evidence cannot determine which record is authoritative. Report the conflict rather than selecting the most recent prose automatically.

### 10.2 Package 2: unified evidence retrieval

Build a bounded index over approved current SDK and product evidence sources, linked to existing symbol/field IDs. Include corrections and negative findings with their scope.

Acceptance: the native-clear example returns the helper, restricted contract, tests and current evidence links; replay queries surface recent relevant records alongside unresolved historical findings; each result identifies source and freshness; missing evidence is not represented as a pass.

Start with a small source set and expand based on demonstrated misses. Exclude credentials and protected private material from indexing.

### 10.3 Package 3: search and response efficiency

Measure representative current queries, then add indexing and consistent response limits. Keep an explicit fallback for missing/stale indexes.

Acceptance: query answers preserve relevant evidence; total/returned counts and truncation are accurate; graph expansion is controlled; malformed and no-match requests produce predictable responses; changes to source content are detected.

Do not claim a speedup until the same questions are measured before and after under recorded conditions.

### 10.4 Package 4: verification separation

Inventory suite dependencies and generation side effects. Expose verification coverage clearly through the canonical CLI and preserve generated evidence.

Acceptance: checking does not silently refresh authoritative inputs; each suite executes once per intended gate unless repetition is deliberate; one final result follows all checks; failures propagate; full-gate requirements remain satisfied.

### 10.5 Package 5: capability contracts

Select a small set of repeatedly used operations, classify their safety/maturity and link current implementation, consumer and evidence records.

Acceptance: a new LLM can determine whether an operation is observational, guarded or expert; examples respect ownership/thread/lifetime rules; retries and uncertain results are documented; native acceptance is scoped to actual evidence.

### 10.6 Package 6: lifecycle scenarios

Start with one recurring failure family, such as session reentry or replay with retirees. Extend existing fixtures and capture tools instead of creating a separate automation stack by default.

Acceptance: runs produce identifiable, comparable evidence bundles; expected invariants are explicit; missing transitions and stale captures are reported; offline replay and real-game acceptance are distinguished; human validation remains recorded separately.

## 11. Measures of improvement

Use a small set of real tasks rather than a large new measurement program. Establish a baseline before implementation and repeat only the relevant checks afterward.

| Measure | What it tells us | Important limit |
|---|---|---|
| Commands needed to reach the correct evidence | Retrieval friction | A shorter route must still find the right evidence |
| Time to a justified first change | Orientation and investigation overhead | Task complexity varies |
| Reopened previously answered questions | Knowledge reuse | Some questions legitimately require fresh evidence |
| Response size and follow-up expansions | Context efficiency | Omitted limitations are not an improvement |
| Focused and full-gate duration | Verification overhead | Coverage must remain explicit |
| Regressions caught by scenarios | Practical test value | Counts alone do not measure severity |
| Status contradictions detected | Current-state reliability | Fewer alerts can reflect weaker checks |

No baseline latency, speedup multiplier or completion percentage is claimed by this draft. The preceding review did not run a controlled performance benchmark.

## 12. Practices to retain and changes to defer

Retain the sole supported CLI, broker ownership, fail-closed checks, evidence tiers, preservation policy, exact artifact tracking and explicit human validation. These protect both the game and the reliability of the knowledge base.

Defer another broad directory reorganization, indiscriminate whole-EXE prose generation, automatic trust promotion, broad wrapper-count expansion and a new knowledge platform without a demonstrated retrieval need.

Avoid turning this roadmap into more mandatory paperwork than it removes. Reuse maintained records, generate views where possible, and add fields only when they answer a repeated operational question. The useful outcome is less reconstruction work for the next agent and better evidence for the next decision.

## 13. Evidence references

These are links to inspected sources or the dated evidence record. Internal implementation files are linked for review, not as supported execution entry points.

| Reference | Relevance |
|---|---|
| [Original Codex review](LLM_WORKFLOW_REVIEW_CODEX_2026-09-30.md) | Commands, counts, findings and verification limits |
| [SDK README](README.md) | Current entry point and historical additions |
| [LLM operating contract](LLM_OPERATING_CONTRACT.md) | Supported execution and evidence rules |
| [CLI implementation](../f1c_mod.py) | Status, doctor, query and suite behavior |
| [Source explorer](../source_explorer.py) | Corpus coverage, scanning and inspection output |
| [Evidence backend](../ask.py) | Existing joined knowledge stores |
| [Tool catalogue](F1C_TOOL_CATALOG.json) | Jobs, effects and tool-selection metadata |
| [Automation checkpoint](AUTOMATION_CHECKPOINT.json) | Current-summary drift and dated evidence |
| [Broker ABI](f1c_broker_abi.h) | Declared ABI and capability interface |
| [Broker discovery](f1c_broker.c) | Negotiation and module lifetime |
| [Reviewed wrappers](f1c_reviewed_api.hpp) | ABI-only direct-call surface |
| [Native-clear helper](f1c_vehicle_native_clear.h) | Retrieval case study |
| [Penalty issue helper](f1c_race_penalty_issue.h) | Queued/completed/uncertain distinctions |
| [SDK test gate](tests/run_all.bat) | Generation, repeated checks and message ordering |
| [Broker penalty test](tests/test_broker_penalty_hold.py) | Actual-implementation regression example |
| [Native replay test](tests/test_replay_live_instances.py) | Captured native bytes and stub limitations |
| [Variant test](tests/test_variant_resolver.py) | Synthetic relocation evidence |
| [Memory MCP guide](F1C_MEMORY_MCP.md) | Observation contract, freshness and limits |
| [Workspace pipeline](../../../governance/PIPELINE.md) | Lifecycle and validation authority |
| [Product status](../../../governance/PRODUCT_STATUS.json) | SDK stage and product distinctions |
| [Development instructions](../../../development/AGENTS.md) | Owner's mod startup compatibility standard |

## 14. Drafting record and outstanding validation

This document was requested as a thorough Markdown expansion of the preceding assessment. It preserves that assessment as the evidence source rather than replacing it. The separate review encountered during the earlier task remains untouched; its timing or full-suite claims are not incorporated here.

The drafting task reread the required workspace/component orientation, ran startup and routing, read the original Codex report, and reran the workspace validator. It made documentation changes only: this new draft and one additive SDK README link. No game, SDK source, IDB, production or lifecycle-stage change was made. No full SDK gate was run because this task did not change implementation and that gate regenerates artifacts.

Before the README edit, its complete bytes were preserved at workspace-relative `workspace_organization_audit/LLM_ASSESSMENT_DRAFT_BEFORE_2026-09-30/f1c_ida_decompiled/tools/sdk/README.md`. Source and backup SHA256 matched `0EC132EF445B58F8C1E5920B12C7075F5D788475C08DA8A3FC27B36C28D247B4`. That backup maps directly to `f1c_ida_decompiled/tools/sdk/README.md`. A future restoration should preserve the then-current file and reconcile only this task's additive index link; retain the draft and review records.

Pending: implementation selection/specification, representative retrieval benchmarks, SDK capability promotion, native scenario execution and human acceptance. Draft completion does not satisfy those future gates.

Documentation closeout will verify that this draft exists, is indexed, has working local reference links, distinguishes evidence from proposals and names the pending work. The pre-edit validator baseline was 11 errors and zero warnings.

### Wasted-effort check

- Repeated large reads: mandatory orientation repeated existing context; one combined output was truncated. The draft used the existing bounded evidence record instead of repeating the source investigation.
- Unnecessarily broad builds/tests: none; no regeneration or runtime suite was invoked for drafting.
- Avoidable failed/retried commands: none material during drafting.
- Polling/waiting/idle: none observed.

### Completed documentation gate

The draft and SDK README index link were verified. All 22 local Markdown references resolve. The README differs from its preserved before-state only by the intended additive draft link. Evidence categories and pending implementation/native validation are explicit. The post-edit workspace validator reports the same 11 baseline errors and zero warnings; no new error was introduced by this documentation task.
