# Community game-research collector: independent review brief

Prepared 2026-10-02 for an independent LLM reviewer. Owner: Matias. Status: proposal for assessment; implementation has not started.

## 1. Your assignment

Determine whether a community-operated F1 Challenge research collector would materially improve our reverse engineering and SDK, and decide what, if anything, should be built first. Reach your own conclusion from the evidence. You may recommend proceeding, narrowing the idea, choosing a different architecture, postponing it, or abandoning it.

Treat the previous assistant's feasibility assessment as a hypothesis. Do not assume that reuse of the Race Data Collector, targeted campaigns, a plugin, a central server, or the proposed delivery order is the best answer. Explain which claims you independently checked, which you dispute, and which remain unknown. Lack of evidence is not evidence of absence.

This is a review assignment. Return a decision and supporting analysis; this brief does not request installation, game changes, deployment, publication, or implementation. If you have workspace access, read its applicable instructions and perform proportionate read-only checks. If you do not, use the evidence summaries below and identify precisely which conclusions require source access or measurements.

## 2. Owner's idea and intended outcome

The owner's original question:

> could we create some sort of an automatized collector which is plugged into the game as a plugin, which continuously collects, harvests and compares data from the game and all the known memory addresses, meaning that if the collector was ran by many people at once, we could harvest a LOT of game data and features

The intended benefit is faster discovery and understanding of game behavior through many people playing different tracks, cars, seasons, settings and scenarios. Evaluate the full ambition, including opportunities to discover behavior that we do not already know how to label. Do not reduce the goal to a race-results exporter.

Possible interpretations to distinguish:

- Gather more observations of known fields to characterize behavior and find failures.
- Observe safely accessible but semantically unknown fields to discover candidates.
- Learn which native functions execute, in what order and with which arguments.
- Infer causal mechanisms or identify safe commands that can become SDK features.

These require different instrumentation and evidence. Determine how far a passive collector can reasonably go, and where additional static analysis, tracing or controlled in-person tests are necessary.

## 3. Current F1C context

The workspace contains a reverse-engineered corpus, a canonical SDK, a shared broker, multiple DLL mods, and a native Race Data Collector. The SDK front door is `python tools/f1c_mod.py`, run from `f1c_ida_decompiled`. It exposes prior-art retrieval, source inspection, typed IDA queries, runtime observations, trace comparison, memory correlation, variant resolution and mod workflows.

As observed in this conversation: broker ABI 15; SDK catalogue 6,383 functions and 124 vehicle fields. These are catalogue counts, not a claim that every entry is callable or every field is understood. Some corpus names/layouts remain legacy research leads. SDK start reports a fresh checkpoint, but that does not establish freshness of every individual document or installed binary.

Recorded product stages: SDK implement/candidate; Race Data Collector verify/candidate. The collector has 0.2.6 installation records, but no installed binary was inspected during preparation of this brief. Source/documentation and live installed state must remain distinct.

The owner previously deferred a guarded scenario runner and the extra-Safety-Car re-entry fix; testing currently stays in person. Passive community observation may be evaluated without treating those deferrals as permission to resume automated gameplay or repair another product.

## 4. Evidence ledger

The labels below identify the basis of a claim. A source fact means code/text was inspected; it does not establish correct native behavior.

| ID | Claim and evidence | Limits / independent check |
|---|---|---|
| E1 | `src/plugin.cpp:22-31` declares an eight-entry `RawFrame` queue containing copied session/driver observations. | Reuse and performance are unproven. Inspect memory size, locks and lifecycle assumptions. |
| E2 | `src/plugin.cpp:48-92` samples through broker session/vehicle interfaces, compares snapshot sequences, records lock/queue drops, and also reads a full fixed-size vehicle buffer before decoding supplemental fields. | It is not solely a thin wrapper over broker snapshots. A bulk read existing in this code does not establish that arbitrary regions are safe to read or upload. Check coherence of the refreshed fields and boundary callbacks. |
| E3 | `src/plugin.cpp:136-162` creates storage/collector objects, connects a journal callback and drains frames in the worker loop. | Existing copying/worker separation is a foundation, not a completed performance or concurrency audit. |
| E4 | `docs/DATA_COVERAGE.md` documents nominal 10 Hz observations, unavailable fields, provisional semantics, gaps and incomplete histories. | Nominal rate is not guaranteed per-field/event resolution. The document is a coverage claim; check source and recorded captures. |
| E5 | `src/plugin.cpp:124-132` records the EXE hash, says collector EXE checks are disabled, and requires compatible broker capabilities. | This records a historical owner exception, not proof that current general startup rules are satisfied. |
| E6 | `development/AGENTS.md` requires supported Lightloader host-protocol verification and required code-pattern/layout validation; whole-file EXE hash gating is not the policy for new mods. | Review the scope/history of E5 against this later instruction. Recommend an interpretation or identify the exact owner decision needed; do not silently alter the existing collector. Hashes remain useful provenance. |
| E7 | SDK `start --json` reports the deferred objectives; the broker header declares ABI 15 and lifecycle/session/vehicle/event capability flags. | Capability names alone do not prove all desired events, semantics or target variants are implemented and accepted. |
| E8 | Existing component records describe race exports/journals and Narrative consumers. | No shared community research ingestion system was established by the prior review. Search before assuming a new service is necessary. |
| E9 | No community pilot, collection overhead benchmark, new unknown-field discovery or cross-user analysis was performed for this proposal. | Technical feasibility, research yield, volunteer demand and operational cost remain hypotheses. |

Source line numbers refer to the revision read on 2026-10-02 and may drift. Hashes are provided in section 12. Earlier README/status sections preserve historical milestones; compare dates and evidence rather than selecting whichever statement supports your conclusion.

## 5. Previous assistant's candidate design — optional, challengeable

The proposed flow was:

`game -> broker-backed plugin -> bounded copy queue -> local spool -> optional upload companion -> validating ingestion -> cross-session analysis -> reviewed SDK findings`

It proposed a baseline stream of reviewed observations plus small research campaigns. Each campaign selects candidate fields with known read boundaries but uncertain meaning, event windows and unrelated control windows. Recipe and interpretation versions are retained. Heavy I/O, compression, networking and analysis run outside the game callback; workers consume copied values rather than dereferencing old live pointers.

Potential metadata includes layout/build identity, recipe/plugin/broker versions, session generation, snapshot sequence, simulation and wall clocks, pause/replay/loading state, entity lifetime, track/settings/mod context, field width/raw value/interpretation, unavailable states and sample gaps.

The suggested sequence was local capture, then a small volunteer comparison, then opt-in shared contribution. This order and the choice to reuse the existing collector have not been independently established as optimal.

Challenge the possibility that targeted campaigns miss unexpected features, that metadata costs outweigh its value, or that a separate collector is easier to validate. Consider a bounded discovery mode that preserves unknown bytes from established object ranges, rotating samples, and baseline controls if it offers worthwhile additional coverage. Distinguish retained raw data from interpreted fields and specify limits. Do not treat rejection of an unrestricted process dump as rejection of all unknown-field exploration.

## 6. Alternatives to compare

| Option | Potential advantage | Main question |
|---|---|---|
| Analyze existing captures only | Lowest runtime/distribution effort; may expose immediate research gaps. | Do current journals retain the necessary raw values and context? |
| Extend Race Data Collector | Existing queue, lifecycle and serialization mechanics. | Would research capture complicate a race-history product or risk its consumers? |
| Separate broker client plugin | Independent schema, budgets and release cadence. | Does it duplicate code, compete for callback budgets or need new broker services? |
| External read-only reader | Existing memory tools; fewer changes inside the game process. | Can it obtain coherent observations and object lifetimes with acceptable deployment friction? |
| Reviewed call/event instrumentation | Can observe events between polls and relate code execution to state. | Are hook sites/ABIs stable and broker-owned; how much perturbation and overhead occur? |
| Local summaries / federated aggregates | Reduced upload and central storage. | Does discarding raw windows make later reinterpretation or independent audit impossible? |

Compare these against the same research objectives. A hybrid is acceptable. Avoid choosing infrastructure or file formats before the required evidence is clear.

## 7. Questions requiring your own reasoning

### Research value

1. Which three concrete unanswered F1C questions would benefit most? Check prior art before selecting them.
2. What additional observation would answer each question, and which facts are inaccessible to passive sampling?
3. Is the main bottleneck lack of data, poor interpretation, incomplete ABI review, native acceptance, or analyst time? Would the collector actually relieve it?
4. How much independent variation would volunteers add? Multiple clients in the same multiplayer race, many frames from one car, and repeated uploads are not independent replications.
5. What would demonstrate failure of the idea or show that a simpler tool is sufficient?

### Measurement and inference

6. How do addresses map to globals, code, object-relative fields, allocated objects and entity lifetimes? What is safely observable before semantic interpretation?
7. Can samples be made coherent? How are mixed ticks, time acceleration, pauses, instant replay, restarts, late join, slot reuse and simulated results detected?
8. How can collection avoid missing short events? A burst triggered after an event cannot reconstruct high-rate history that was never retained beforehand.
9. How will you distinguish an event's cause from its consequence or a common cause? Which hypotheses need static xrefs or a controlled intervention?
10. What protects discovery against thousands of spurious correlations, autocorrelation, selection bias, mod confounding, sentinel values and overfitting? State a holdout/replication and multiple-comparison approach appropriate to the scale.
11. Which observations should never be pooled across different build/layout, track/config or mod combinations? Is full stratification too sparse, and what justified pooling is possible?
12. How will a person audit a proposed discovery from the original records, including disconfirming examples and changed field interpretations?

### Runtime and operations

13. What can reuse existing reviewed SDK services and what requires new native access? Does queue overflow preferentially drop busy incident frames and bias the result?
14. What are the CPU/callback-tail, memory, disk and bandwidth costs? Show a transparent estimate, then say what must be measured.
15. What happens on disk full, unavailable network, process crash, duplicate chunks, partial writes, plugin stop, session teardown and recipe incompatibility?
16. What is the smallest useful contribution package? Which identities/configuration values are necessary, optional or excluded? Pseudonymous IDs are not a promise of anonymity.
17. How will malformed, mistaken, modified or replayed client data be detected? Authenticated submissions establish origin/integrity, not truth of gameplay.
18. Who maintains recipes, schemas, review decisions, storage, access and contributor support? Is that workload justified by the expected research yield?

## 8. Quantitative reasoning to include

Use an explicit workload model instead of unsupported promises of negligible overhead. A starting storage expression is:

`raw bytes/sec = entities * sampled fields/entity * bytes/field * samples/sec + metadata/events`

Illustration only: 22 cars * 100 four-byte fields * 10 samples/sec = 88,000 bytes/sec, or 316.8 MB per hour in decimal units, before timestamps, identifiers, event windows, encoding overhead or compression. This is a hypothetical workload, not a measurement of the existing collector. Bytes read in-process may differ substantially from bytes retained/uploaded, especially with full-object reads or delta encoding. Compression ratio must be measured on real representative data.

Propose budgets as provisional choices and state the hardware/session assumptions. Include aggregate growth across contributors and retained hours, not only an individual session. Explain how rate reduction and quotas affect scientific usefulness and event bias. Avoid a magic minimum participant count without a specific endpoint, event prevalence and independence model.

## 9. Constraints versus open decisions

**Established workspace/owner boundaries:** preserve source and records; no hard deletion; never read credentials/private keys; canonical development locations; reuse broker ownership rather than duplicate hooks; comply with reviewed thread/lifetime contracts; no production changes under this review; in-person native acceptance and explicit release authority; follow SDK front-door and evidence rules. Read authoritative instructions for exact scope. A conflict affecting protected state/lifecycle must be raised, not silently resolved by prose.

**Proposal choices open to revision:** extending this collector versus a separate client; baseline/campaign strategy; recipe format; file encoding; sampling frequencies; local versus server-side analysis; whether to build a dashboard; central uploads versus local summaries; initial volunteer count; exact metadata; retention defaults; delivery order.

**Not established:** whether arbitrary known addresses can be read safely; that a source label is correct; that broader telemetry yields causal explanations; that the public can run the current build safely; that the current collector meets later startup rules; that remote upload can be added without meaningful maintenance cost; that a fixed sampling rate captures all relevant transitions.

## 10. Required review deliverable

Write a self-contained assessment with:

1. **Decision:** proceed, narrow, alternative, postpone or reject; state confidence and what evidence could change it.
2. **Evidence audit:** verified source facts, corrections to this brief/earlier design, unresolved conflicts and inaccessible evidence. Cite paths/symbols/lines or primary URLs.
3. **Value case:** three specific research questions, expected learning, current bottleneck, and comparison with analyzing existing data or conducting a focused manual test.
4. **Architecture decision:** compare credible alternatives, identify required SDK changes, ownership/coherence boundaries and effects on current consumers.
5. **Capture and inference contract:** minimum record/recipe fields, sampling/control/holdout strategy, handling of unknowns and tiers, and criteria for a useful research lead versus a verified SDK capability.
6. **Cost and reliability assessment:** transparent estimates, assumptions, measurements needed and important failure modes, prioritized by expected impact.
7. **Smallest decisive pilot:** exact scope, inputs, expected positive and negative examples, comparison baseline, acceptance/abort conditions and required human validation. A pilot may be analysis-only if that best resolves uncertainty.
8. **Open decisions:** distinguish decisions you can justify now, questions requiring new evidence, and choices that genuinely belong to Matias.

Do not manufacture precise estimates, interpret missing values as zero, promote correlation to causal proof, or call a source inspection a runtime test. Record negative findings. A good review can conclude that the collector is feasible but not currently worth building.

## 11. Source map and workspace review entry

Workspace root: `%USERPROFILE%\Desktop\F1`. Paths below are relative to that root; the links resolve from this document.

| Read | Purpose |
|---|---|
| [Root instructions](../../../../AGENTS.md), [README](../../../../README.md), [governance](../../../../governance/README.md), [pipeline](../../../../governance/PIPELINE.md), [product status](../../../../governance/PRODUCT_STATUS.json) | Authority, preservation and current lifecycle. |
| [Development instructions](../../../AGENTS.md) | Canonical paths and September 24 host/layout contract. |
| [Collector README](../README.md), [status](../STATUS.md), [coverage](DATA_COVERAGE.md) | Existing purpose, recorded history and unavailable/provisional data. |
| [plugin.cpp](../src/plugin.cpp), [collector.cpp](../src/collector.cpp), [storage.cpp](../src/storage.cpp) | Sampling, queue, lifecycle, event interpretation and serialization. |
| [No EXE gates record](NO_EXE_GATES_2026-09-15.md), [RF audit](RF_AUDIT_2026-09-23.md), [multiplayer assessment](MULTIPLAYER_ASSESSMENT_2026-09-20.md) | Compatibility history and known native-validation limits. |
| [SDK instructions](../../../../f1c_ida_decompiled/AGENTS.md), [operating contract](../../../../f1c_ida_decompiled/tools/sdk/LLM_OPERATING_CONTRACT.md), [current state](../../../../f1c_ida_decompiled/tools/sdk/CURRENT_STATE.md) | Supported discovery workflow, tiers and deferred work. |
| [Broker ABI](../../../../f1c_ida_decompiled/tools/sdk/f1c_broker_abi.h), [race observation review](../../../../f1c_ida_decompiled/tools/sdk/RACE_DATA_OBSERVATION_REVIEW.md) | Available contracts and supplemental-field evidence. |
| [Earlier collector design](COMMUNITY_RESEARCH_COLLECTOR_DESIGN_2026-10-02.md) | Prior assistant's proposed answer; read critically after forming initial hypotheses. |

After mandatory orientation, use the supported front door from the SDK repository:

```powershell
python tools/f1c_mod.py start --json
python tools/f1c_mod.py evidence-query findings "collector" --json
python tools/f1c_mod.py capabilities --json
python tools/f1c_mod.py route PATH
```

Then select bounded inspect/query operations relevant to your hypotheses. RECORDED checkpoint summaries require checking their evidence links. Do not execute internal helpers or start a live reader merely to write a design review. If a runtime observation is necessary, follow its canonical guide and report freshness and what was actually observed. This document grants no new runtime permission.

## 12. Reproducibility and preparation record

Source hashes at preparation (SHA-256):

| Path | SHA-256 |
|---|---|
| `development/mods/race_data_collector/src/plugin.cpp` | `69a832836802c0859fc982555c3d458cce8cb3e49c9824809303ceb2345ed9f6` |
| `development/mods/race_data_collector/src/collector.cpp` | `9e3e36555227403b2087140961f94f166d1d0799de46e496a9656474937d5997` |
| `development/mods/race_data_collector/src/storage.cpp` | `7fdc624cd247f8ebcbfea7f95f61455d3429eef8868bc2961be9092f9cb12ffe` |
| `development/mods/race_data_collector/docs/DATA_COVERAGE.md` | `5062d65c83413b54b435c331c91a3bca622cbfeb1fae6cc9e2cccb7d0faa0267` |
| `f1c_ida_decompiled/tools/sdk/f1c_broker_abi.h` | `18ad04db10d7b41130428c2ab498cd257345f49c59e4433be04b6e810d542c67` |
| `development/AGENTS.md` | `5f055f936bdbdf88406ad864ce002418d4db0100ebaa9b6c1b1a016a8d5f2247` |

Preparation verified the earlier design and indexed findings, fresh SDK start, selected sampler/worker source and these hashes. Workspace validator initially passed with zero errors/warnings. No gameplay, performance, networking, build or community-analysis test was run. This brief is not an independent review of itself.

Changes are this brief, one component README link, and a source-evidence clarification in SDK findings. Before-states are under `workspace_organization_audit/COMMUNITY_COLLECTOR_REVIEW_BRIEF_BEFORE_2026-10-02/`. Restoration must merge only these additions while preserving concurrent work; records are not hard-deleted. Existing design, product stages and objectives are retained. Pending validation is the independent assessment and any measurements it identifies.

Documentation closeout checks cover link resolution, discoverability, UTF-8 without BOM, source-hash consistency, appended finding parse/retrieval and workspace structure. No SDK tests are appropriate to this document-only change. Wasted-effort check: focused design/source rereads only; no broad builds/tests; no avoidable failed/retried commands; no polling or idle waits observed.
