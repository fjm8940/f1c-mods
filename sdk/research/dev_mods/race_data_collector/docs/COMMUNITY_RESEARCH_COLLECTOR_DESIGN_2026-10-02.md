# Community research collector: feasibility and initial design

Date: 2026-10-02, local time. Product anchor: Race Data Collector (verify/candidate in PRODUCT_STATUS); associated SDK remains implement/candidate. This document defines a proposed research collection mode, not an implemented feature or a stage change.

## Objective

Owner asks whether a plugin continuously harvesting and comparing game memory across many users could accelerate F1C research. Yes: distributed, contextual observations can broaden scenario coverage and expose reproducible patterns. They cannot automatically establish unknown field semantics, causal behavior, safe native commands or complete feature access.

Success would mean reusable, bounded, provenance-bearing observations collected during ordinary play, cross-session comparison with explicit missingness, and reviewable research leads that link back to the SDK knowledge store. No upload, deployment, installation, gameplay automation or new runtime hook is authorized/performed by this feasibility task.

## Verified local foundation

FACT from source/documents: existing src/plugin.cpp already receives broker-backed session/vehicle snapshots in callbacks, copies bounded RawFrames into an eight-entry queue, tracks drops, and drains the queue on a worker. Existing collector/storage layers create event journals and race exports. docs/DATA_COVERAGE.md describes nominal 10 Hz telemetry, capture gaps and provisional field semantics. This is a substantial implementation starting point, not proof of customer-safe research collection or multiplayer correctness.

The collector's purpose currently is race history/Narrative data. Continuous unknown-field research, cross-user ingestion and analysis are additional requirements. Readme/status contain historical contradictory version/guard statements; latest linked 0.2.6 records and inspected source supersede older milestones for this assessment. Source logs that EXE checks are disabled; existing installation must not be represented as meeting the September 24 host/layout startup contract without a dedicated review. No silent compatibility repair or reversal of the September 15 owner exception is made here. A new distributed research mode must explicitly resolve its compatibility contract before release.

SDK start is fresh (stale=false), broker ABI is 15, and the canonical interface has reviewed read-only snapshots/observations, trace comparisons and memory correlation. The curated collector prior-art query currently yields an older RECORDED checkpoint; its claims were checked through component docs/source, not upgraded by the checkpoint itself.

## Proposed architecture

Game -> supported Lightloader/broker -> bounded research sampler -> bounded queue -> local spool -> optional consented companion upload -> validating research ingest -> cross-session analysis -> reviewed findings.

The game-thread part only copies reviewed observations within a measured budget. Compression, file I/O, comparisons and networking occur outside the game callback. Worker code never dereferences live game pointers captured earlier; snapshots are value copies. Queue saturation drops observations and records loss, rather than blocking play. Stop/unregister/reset order must respect in-flight callbacks and object/session lifetime.

The plugin and companion share versioned schemas. Initial releases keep collection local, with manual export and inspection. A separate companion can later upload opt-in batches and handle retries/deduplication; no remote ingestion service currently exists for this design. Collection should continue without a connection. A crash cannot guarantee the most recent buffered data survives, and that loss must be explicit.

## Two capture modes

1. Baseline: reviewed session and vehicle fields, weather/surface observations, lifecycle, lap/pit/damage/retirement/penalty/race-control transitions and known feature-provider state. Capture only currently available reviewed capabilities; missing fields are unavailable, never zero-filled guesses.
2. Targeted research campaign: a small, locally validated recipe chooses a bounded set of candidate offsets inside reviewed live object ranges, width/type, lifetimes, allowed phases and event triggers. Candidate interpretation remains OPEN/DERIVED. Rotate campaigns among participants to broaden coverage without reading everything on each PC.

Executable function addresses are code locations, not telemetry values. Vtables, globals and heap objects have different read contracts. Some pointers refer to destroyed objects; some apparently readable memory is stale; some values are sentinel/derived/render-only state. Therefore the catalogue is an inventory of leads, not a safe scan list. Arbitrary process dumps, remote address commands, speculative pointer traversal and calling every known function are excluded from this design.

A recipe update changes what the plugin reads. Treat it as reviewed/versioned configuration, with authenticity/integrity and strict local allowlist/budget validation, not remotely supplied executable code. Authentication alone does not establish that a recipe is safe.

## Observation record

Each chunk records schema/plugin/broker/recipe versions; executable build fingerprint and validated layout profile; pseudonymous capture/session IDs; session generation and snapshot sequence; wall and simulation clocks; pause/replay/loading/session phase; driver entity lifetime IDs; track/layout, season and relevant mod/config fingerprints; field ID, width, raw value and available unit/interpretation; source/tier; unavailable/invalid states; sampling rate, drops and gaps. Raw addresses may be included in developer-only recipe metadata but are never entity identity across machines/sessions.

Cross-participant driver/car identity should be stable within the capture, without uploading player names/account identifiers or absolute local paths by default. Include only allowlisted configuration/context values. No entitlement material, passwords, chat, licence keys or arbitrary memory strings. Contributions use explicit opt-in sharing with a local preview and a way to stop collection; server-retention/access choices require definition before hosting. These are concrete requirements for voluntary community evidence, not a general secret scan or existing product claim.

## Sampling and event windows

Use configurable tiers: infrequent static/session metadata, moderate-rate baseline snapshots, and brief higher-rate candidate windows around reviewed transitions. Exact rates and CPU/disk caps must be measured on slower machines/full grids before fixing release defaults. The existing nominal 10 Hz is a starting reference, not evidence that 10 Hz resolves every event.

Maintain a bounded pre-event ring and collect a bounded post-event window. Keep unrelated control windows to measure background variation. Retain periodic baseline frames so event-only sampling does not hide counterexamples or create unmeasured selection bias. Rare events that happen between polls can be missed; faster bursts help only after a trigger exists. Existing broker event capabilities should be used where available; new hook/call-tracing instrumentation requires its own review and must not duplicate broker-owned hooks.

Use delta/change encoding, compression and duplicate-chunk IDs where useful. Constant fields should not be reserialized at full rate. Storage is capped and reported; dropping/rotating per-run data follows applicable retention rules. In this workspace, per-run output belongs in TEMP and removal only through retention quarantine; distributed client retention rules need their own explicit product policy. More addresses x cars x samples x sessions rapidly creates substantial traffic, so no unbounded raw retention claim is acceptable.

## Comparison and research output

Partition data by compatible layout/build, recipe, track, settings, mod stack and session mode. Compare event windows against matched controls, field change timing, distributions, state transitions and counterexamples. Report cohorts, independent contributing sessions/installations, missingness and confidence separately from raw sample count. Ten thousand samples from one car are not ten thousand independent experiments.

Example hypothesis: a candidate field rises before pit entry. Test how often it changes during actual pit requests versus ordinary racing, whether it also changes for penalties/replay, and whether the relationship survives unseen tracks/sessions. It remains a lead until code/xrefs identify ownership and meaning and a focused in-person test validates the interpretation. A field changing alongside retirement might be cleanup output, not the retirement command.

The ingest rejects malformed/out-of-order/duplicate/impossible records and retains provenance for suspect contributions. Contributor/version stratification and held-out sessions limit contamination from modified clients, correlated mod behavior and overfitted discovery. Signing transport does not make reported gameplay trustworthy. No automatic promotion from statistical correlation to FACT, ABI review or safe command support.

Aggregate findings link back to specific capture manifests and field recipes. Store proven, disproven and open results in the canonical SDK findings store; large capture chunks are data by-products rather than permanent prose. Never silently reinterpret historic raw values after an SDK name changes; interpretation versions stay attached.

## Practical learning opportunities

Likely useful: tyre/fuel/weather distributions across tracks and cars; rare pit/failure/retirement state sequences; Safety Car/Red Flag lifecycle and session re-entry counterexamples; AIW-relative positions at incidents; variant compatibility and sentinel ranges. These observations can prioritize reverse engineering and regressions, but cannot create native functions or resolve opaque rendering/resource ownership solely by collecting more samples.

A campaign dashboard should emphasize missing coverage and disconfirming examples, not simply a large data total. It can assign different small recipes to users while recording assignment and baseline context. Avoid deliberately provoking failures or altering gameplay in the normal community collector.

## Staged delivery and acceptance

1. Local research mode: reviewed baseline plus one candidate campaign; source/layout/host contract, consent UI and bounded spool. Acceptance includes unsupported target/refused capabilities, session lifetime reuse, pauses/replay, disk failure, queue overflow and clean shutdown.
2. Analysis pilot: compare captures from Matias and a small volunteer group with different track/settings coverage. Independent parser checks, known positive/negative controls, withheld-session validation and reproducible reports precede claims of discovery.
3. Community contribution: opt-in companion upload, validated ingestion, deduplication, quotas, schema/version handling, data-access/retention choices and distribution documentation. Deployment/release authority and exact-artifact in-game acceptance remain separate.

Measure full-grid callback tail latency, frame-time regression, CPU, disk and compressed bytes per hour; record target hashes, installed pair and scenario. No performance figures or thresholds are claimed by this feasibility review. Proposed abort conditions: inability to distinguish loading/replay/lifetime, blocking callback I/O, unresolved compatibility authority, stale or unsafe recipe reads, silently lost data, incorrect cohort mixing or unintended identifier disclosure.

This reuses the collector's mechanics while keeping its race/Narrative output schema stable; research uses a distinct versioned stream and namespace. No implementation plan should overwrite existing event/export meanings to fit a new research use.

## Evidence, changes, restoration and pending work

Performed: component AGENTS/README/status and coverage reads; inspected sampler/queue/worker source and broker contract; SDK start/prior-art/route and bounded inspect; workspace validator initially PASS 0/0. No live memory reads, plugin builds, installation, networking or tests occurred. Feasibility and proposed architecture are DERIVED; local implementation inventory is FACT about source, not gameplay validation.

Changed: this new design, its component README link and two SDK findings entries. Complete pre-edit component README and findings bytes were preserved/equality-checked under workspace_organization_audit/COMMUNITY_COLLECTOR_DESIGN_BEFORE_2026-10-02/ with SHA-256 manifest. Restore by merging only this task's additions out and preserving concurrent/subsequent changes; no hard deletion. Product stage, SDK objectives, deferred testing automation and all installed/production files remain unchanged.

Pending: owner chooses implementation scope; compatibility exception/September 24 contract reconciliation; exact initial field/recipe allowlist; measured budget and retention defaults; volunteer contribution UX and server/data ownership choices; independent capture/analysis validation and in-person gameplay acceptance. This feasibility request does not supply those choices or authorize publication.
