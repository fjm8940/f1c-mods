# Delegated SDK execution plan

Date: 2026-08-06

This plan converts the remaining SDK roadmap into bounded work that cheaper
models can perform while a frontier orchestrator controls architecture, live
experiments, and promotion. It complements `SDK_COMPLETION_ROADMAP.md`.

## Operating rule

Worker output is evidence or a candidate, never fact. Nothing becomes public
SDK surface until it passes deterministic validation, independent review,
orchestrator approval, and the required runtime tests.

## Model roles

| Role | Suitable model | Responsibility | May promote SDK facts? |
|---|---|---|---|
| Extractor | Cheapest capable coding model | Search functions, callers, offsets, strings, layouts, and existing evidence | No |
| Candidate author | Cheap coding/reasoning model | Produce typed candidate contracts, signatures, tests, docs, or isolated code | No |
| Independent reviewer | Separate cheap model/context | Try to disprove candidates and identify missing evidence | No |
| Reconciler | Stronger cheap model | Compare candidates and emit explicit agreements/disputes | No |
| Orchestrator | Frontier model | Select boundaries, resolve disputes, design public APIs, authorize probes, and promote | Yes, after gates |
| Runtime harness | Deterministic tools plus orchestrator | Build, launch, measure, rollback, and preserve raw logs | Produces evidence only |

Workers do not need the full repository. Each receives a small evidence packet
containing exact functions/files, relevant callers/callees, required schema,
known facts, prohibited assumptions, and an output-size limit.

## Standard worker pipeline

1. Orchestrator selects one narrow capability and freezes its evidence packet.
2. Two independent extractors produce grounded findings.
3. One candidate author creates the contract or isolated implementation.
4. Deterministic checks validate addresses, numeric widths, schemas, builds,
   citations, dependency cuts, and forbidden production modifications.
5. An independent reviewer searches for counterexamples.
6. A reconciler emits only `AGREED`, `DISPUTED`, or `INSUFFICIENT_EVIDENCE` per
   claim, with source references.
7. The orchestrator reviews only the compact reconciliation and disputed raw
   evidence.
8. A bounded live probe runs with explicit preconditions, timeout, rollback,
   cleanup, and a machine-readable result.
9. Regression, unload, conflict, stale-handle, and second-session gates run.
10. The orchestrator either promotes the capability or records the blocker.

For high-risk mutation, networking, lifecycle, allocator, and hook work, use
three independent candidates instead of two.

## Remaining phases

### Phase 1 — lifecycle closure

Deliverables:

- Observe garage, loading, race start, restart, exit, and a second session in
  one process.
- Define authoritative phase transitions and object-validity windows.
- Prove command rejection during teardown and stale-token rejection after
  session replacement.

Delegation:

- Cheap workers trace lifecycle globals, callers, and existing logs; propose
  state-machine candidates and test matrices.
- The orchestrator chooses anchors, designs the live probe, and interprets
  ambiguous transitions.

Gate: two complete same-process cycles with clean unload and no stale access.
Expected cheap-model share: **65%**.

Progress 2026-08-06: one game-owned mini-restart produced two active sessions in
one process, generation 1 -> 2, and old-generation rejection. A separate run
reached the interactive qualifying garage and produced one natural full-weekend
exit with RestartGate return 6 and clean hook detachment. These results close
the exercised single-player lifecycle path; replay and multiplayer remain
explicitly unproven rather than inferred.

### Phase 2 — broker discovery and compatibility

Deliverables:

- Stable broker discovery independent of a guessed load path.
- ABI version and capability negotiation.
- Single-owner election, incompatible-version refusal, and documented startup
  ordering.

Delegation:

- Cheap workers compare Win32 discovery mechanisms, implement isolated
  candidates, and generate compatibility tests.
- The orchestrator selects the trust and ownership model.

Gate: old/new/missing/duplicate broker matrix passes fail-closed tests.
Expected cheap-model share: **75%**.

### Phase 3 — generation-safe handles and snapshots

Deliverables:

- Replace persistent raw game pointers with slot-plus-generation handles.
- Immutable vehicle/session/timing/weather/rules snapshots.
- Explicit freshness, width, units, and thread rules.

Delegation:

- Cheap workers inventory fields, generate layouts/serializers, and build stale
  handle tests.
- The orchestrator decides identity semantics and snapshot boundaries.

Gate: slot reuse, teardown, concurrent reads, and second-session tests pass.
Expected cheap-model share: **80%**.

### Phase 4 — public event system

Deliverables:

- Bounded events for lifecycle, simulation update, vehicle ownership, lap, pit,
  damage, failure, and race control.
- Subscription generations, quotas, ordering rules, and safe unsubscribe.

Delegation:

- Cheap workers trace candidate event sources, implement queue variants, and
  generate ordering/backpressure tests.
- The orchestrator defines event semantics and selects authoritative sources.

Gate: hostile subscriber, unload, reentrancy, ordering, and dropped-event
accounting tests pass.
Expected cheap-model share: **75%**.

### Phase 5 — read-only subsystem coverage

Deliverables:

- Typed observation APIs for vehicles, timing, track, weather, rules, camera,
  input, pit state, damage, and AI state.
- Units, enums, validity masks, and variant metadata.

Delegation:

- This is the largest cheap-worker wave: independent field tracing, cross-log
  correlation, schema generation, docs, and smoke tests.
- The orchestrator reviews disputed meanings and public naming.

Gate: every exposed field has width, owner, lifecycle, natural runtime evidence,
and an unknown-value policy.
Expected cheap-model share: **90%**.

### Phase 6 — semantic command waves

Process separate waves in this order:

1. Camera and presentation commands.
2. AI and pit commands.
3. Weather and track-state commands.
4. Damage and mechanical-failure commands.
5. Race-control and vehicle-placement commands.
6. Input and player-control commands.

Each command needs copied arguments, broker-owned execution, validation,
transaction/rollback behavior where possible, and no retained client pointers.

Delegation:

- Cheap workers identify candidate leaves, reconstruct call contracts, generate
  negative tests, and implement isolated broker commands.
- The orchestrator approves every ABI, side-effect boundary, and live mutation.

Gate per command: static ABI review, natural observation, bounded mutation,
rollback/unload, lifecycle negatives, conflict test, and documented example.
Expected cheap-model share: **70–80%**.

### Phase 7 — resource and content interception

Deliverables:

- Owned file/archive lookup interception.
- Safe replace/add paths for configuration, cars, tracks, championships, rules,
  textures, models, sounds, and other assets.
- Conflict ordering, cache invalidation, and hard-limit reporting.

Delegation:

- Cheap workers map loaders and formats, build corpus tests, and implement
  parsers/validators.
- The orchestrator selects interception points and memory ownership rules.

Gate: vanilla parity, malformed-resource refusal, override conflicts, reload,
and teardown pass.
Expected cheap-model share: **80%**.

### Phase 8 — UI, rendering, audio, and normalized input

Deliverables:

- Overlay/HUD API, normalized input events, camera facade, and audio-event API.
- Device reset, focus loss, resolution change, and unload handling.

Delegation:

- Cheap workers trace subsystem boundaries, create examples, and generate device
  lifecycle tests.
- The orchestrator owns renderer/device threading and compatibility decisions.

Gate: repeated device/session transitions and coexistence with multiple mods.
Expected cheap-model share: **70%**.

### Phase 9 — multiplayer boundary

Deliverables:

- Read-only packet/event observation first.
- Explicit server/client authority checks for any command surface.
- Size, rate, provenance, and malformed-input validation.

Delegation:

- Cheap workers map packet handlers and generate parsers/fuzz cases.
- The orchestrator controls security, authority, and all mutation decisions.

Gate: offline/host/client matrix, fuzzing, disconnect/reconnect, and incompatible
peer behavior pass.
Expected cheap-model share: **60%**.

### Phase 10 — diagnostics, containment, and expert tier

Deliverables:

- Per-mod logging, budgets, crash attribution, hook/conflict map, quarantine,
  permissions, and capability audit.
- Clearly isolated, version-locked raw memory/call/hook escape hatch.

Delegation:

- Cheap workers implement reporting, test fault cases, and generate diagnostic
  documentation.
- The orchestrator defines containment promises and unsupported boundaries.

Gate: deliberately faulty mods cannot silently corrupt broker ownership; expert
tier is visibly unsafe and cannot masquerade as portable SDK code.
Expected cheap-model share: **70%**.

### Phase 11 — executable variants

Deliverables:

- Fingerprints, invariant signatures, relocated functions/operands, layout
  manifests, and refusal rules for unknown builds.
- Regression fixtures for every explicitly supported executable.

Delegation:

- Cheap workers perform bulk binary comparison and candidate signature work.
- The orchestrator approves invariants and decides when a variant is supported.

Gate: every promoted capability passes on each registered variant or is marked
unavailable through negotiation; unknown variants perform no mutation.
Expected cheap-model share: **85%**.

### Phase 12 — packaging, documentation, and SDK v1 freeze

Deliverables:

- Manifests, dependencies, templates, build/deploy tools, examples, reference
  documentation, compatibility policy, and migration guide.
- Repeatable static and live acceptance suite.

Delegation:

- Cheap workers generate and cross-check docs/examples/tests.
- The orchestrator freezes semantics, version policy, and release criteria.

Gate: clean-machine install, sample-mod build/deploy, multi-mod weekend, restart,
exit, second-session, and removal all pass without manual repair.
Expected cheap-model share: **90%**.

## Concurrency and cost policy

- Use **6–12 workers** for implementation/reconciliation waves.
- Use up to **48 workers** only for independent static inventories with no
  shared-file writes.
- Give each worker one bounded deliverable and a maximum output schema; do not
  ask workers to “understand the whole SDK.”
- Workers write candidate artifacts into unique task directories. Only the
  orchestrator merges canonical files.
- Cache function packets and validated evidence so later workers do not reread
  the decompiler corpus.
- Orchestrator reads reconciliation summaries and disputed excerpts, not all
  worker transcripts.
- Stop a wave when new candidates cease adding validated capabilities; do not
  spend workers increasing confidence scores without new evidence.

## Machine-readable tracking

`AUTOMATION_CHECKPOINT.json` remains the single current-state pointer. Each phase
should also maintain a manifest containing:

- task ID and immutable evidence-packet hash;
- assigned role/model and output path;
- deterministic validation result;
- agreements, disputes, and unresolved claims;
- orchestrator decision;
- runtime evidence and cleanup status;
- promoted contract/API version, if any.

Only real state transitions update the checkpoint. Worker count, generated text,
or confidence scores do not count as progress.

## Completion definition

The project is “SDK v1 complete” when phases 1–12 pass their gates for the
declared supported executable set. “Anything is possible” then means:

- safe supported facades cover the major game subsystems;
- unsupported experiments remain possible through an explicit expert tier;
- unknown executables and invalid lifecycle states fail closed;
- no claim of portability or safety depends solely on LLM judgment.
