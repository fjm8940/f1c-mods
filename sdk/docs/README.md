# docs/ — Shareable Research Framework

This folder holds **standalone research papers** about F1 Challenge 99-02 internals,
written to be shared outside this corpus (F1C communities, forums, modding groups).
The reader is assumed to have **no access** to the decompiled game files, the IDB, or
any of the tooling. Every paper must make sense on its own.

## Pipeline — every paper follows this order

```
1. STATIC RESEARCH      decompiled/disassembled code, data references, string keys
2. RUNTIME POKE RESEARC live memory reads/writes on a running game (poke.py et al.)
3. FINDINGS             proven functionality logic, with evidence tier per claim
4. TLDR                 plain-language summary for non-technical readers
```

- A paper may ship with stages 2–3 marked **PENDING** — static-only research is
  shareable, but must say so honestly.
- Never let a later stage silently contradict an earlier one: runtime results
  override static interpretation, and the paper must say where it changed its mind.

## Document form — academic thesis structure

Use `docs/TEMPLATE.md`. Required sections, in order:

1. **Title block** — topic, game version/binary, date, author, status badge.
2. **TLDR / Abstract** — plain language, no offsets, no jargon. What happens in
   practice, why, and how. Written for players and rcd/livery modders.
3. **1. Introduction** — research question, scope, why it matters for play/modding.
4. **2. Method: static analysis** — what code was examined, where the data comes
   from (described, not dumped — the reader has no source).
5. **3. Method: runtime verification** — live experiments: setup, tool described in
   one line, procedure, observations. Mark PENDING if not done.
6. **4. Results / findings** — the proven mechanism, step by step, with an evidence
   tier on every claim (see below).
7. **5. Discussion** — caveats, ambiguities, alternative interpretations, open items.
8. **6. Conclusion** — short answer to the research question.
9. **Appendix A: corrections** — mistakes found in earlier notes/tools while writing.
10. **References** — game version, related papers in this folder, external sources.

## Evidence tiers (label every technical claim)

| Tier | Meaning |
|---|---|
| **FACT** | proven live (poke-observed/poke-written in a running game) |
| **CONFIRMED** | two independent sources agree (e.g. code + community docs) |
| **DERIVED** | grounded in code, not yet runtime-proven |
| **GUESS** | speculation — avoid in shared papers; if needed, flag loudly |

Direction/sign/scale semantics are never better than DERIVED until a live poke shows
the effect. Say which tier each result is.

## File rules

- Naming: `NN-kebab-case-topic.md`, next free number (check folder listing).
- Self-contained: no absolute paths from this corpus, no `tools/...` invocations as
  required reading, no raw source dumps — describe the code in prose/tables/pseudocode.
- Small pseudocode and offset tables are fine and encouraged.
- One topic per paper. Cross-reference siblings by filename.
- When runtime verification lands, update the paper in place and bump the status
  badge; keep a one-line changelog at the bottom.

## Status badges (title block)

`STATIC ONLY` → `RUNTIME VERIFIED (partial)` → `RUNTIME VERIFIED`

## Index

| # | Paper | Status |
|---|-------|--------|
| 41 | `41-raceability-mechanism.md` — how rcd RaceAbility drives AI pace | RUNTIME VERIFIED |
| 42 | `42-ai-realism-and-difficulty-model.md` — the two PLR knobs: AI Strength (pace) and AI Realism (aggression), plus a difficulty calculator | RUNTIME VERIFIED |
| 43 | `43-ai-difficulty-calculator.md` — how the calculator works: player base-speed input, per-track anchoring, consistency loop | TOOL DOC (model RUNTIME VERIFIED) |
| 46 | `46-player-specific-mechanical-failures.md` — which engine.ini / .plr knobs drive the player's engine failures (two split models: AI countdown vs player stress/thermal). **Read its §0 errata first — four claims were overturned by live testing on 2026-07-31** | STATIC, **partially CORRECTED by live test** |
| 47 | `47-engine-map-tokens-live-verified.md` — which of the three engine.ini "map" tokens the game actually reads. `RevLimitSetting` live, `EngineMapSetting` **inert**, `EngineBrakingMapSetting` live and worth ~2× coasting deceleration | STATIC + **LIVE A/B PROVEN** |
| 48 | `48-nt2004-caution-flag-system-port.md` — the NASCAR Thunder 2004 course-yellow/pace-car/red-flag system (same engine lineage) and the F1C port surface in dependency order. Answers: red flag is dormant machinery in both games; yellow overtaking is prevented by pace-line control + corrected by a per-frame geometry check into penalties/lap deductions | STATIC ONLY |
| 49 | `49-formation-lap.md` — the full chain from the `.plr` `Formation Lap` key to the parade lap, grid release, and the green-flag/cooldown gate (`0x7CEE88`). Includes recon settings, AI path-tracking ownership of car positions, and the open question of who sets the green-light gate | STATIC + PARTIAL RUNTIME EVIDENCE |
| 50 | `50-camera-system-static-architecture.md` — camera manager lifecycle, local/static/tracking families, mode and target state machines, common view layout, render boundary, and a bounded runtime backlog | STATIC ONLY |
| 51 | `51-static-analysis-system-priorities.md` — ranks under-mapped systems by static-analysis suitability and prospective mod value; records the camera-first decision, deferred runtime-heavy targets, and missing-knowledge-graph fallback | STATIC ONLY |
| 52 | `52-aiw-navigation-static-architecture.md` — AIW scene/path selection, database parsing, 316-byte waypoint graph, placement arrays, derived-data lifecycle, consumer map, limits, hazards, and a read-only live baseline | STATIC + OBSERVED |
| 53 | `53-ai-pit-stop-procedure-runtime-map.md` — natural AI pit request, entry, service timing, fuel target, tyre/front-wing outcomes, release, direct AIW event-node passage, restart comparison, and remaining consumer questions | RUNTIME OBSERVED + STATIC |
| 54 | `54-ai-driving-brain-map.md` — AI per-tick pipeline, pure-pursuit steering law sub_426100, pedal post-processor, modes 0x4510, native dead-car coast | STATIC |
| 54 | `54-championship-career-static-architecture.md` — `.bch` persistence, 45,396-byte season object, player/opponent records, scoring and 90% eligibility, mode/session ownership, calendar/database coupling, lifetime statistics, constructor aggregation, malformed-save boundaries, and validation backlog | EXHAUSTIVE STATIC + PRIOR LIVE RACE-STATE OBSERVATION |
| 55 | `55-cpu-render-bottleneck-investigation-plan.md` — phased plan to verify where F1C frame CPU time goes: `VBSTRATEGY` semantics, D3D8 device behavior flags and draw path, threading model, runtime measurement protocol, ranked fix candidates. Seeded by an external modder report; linked 2026-10-13 findings establish the strategy/device/buffer map and a network-worker creation path | PARTIAL STATIC; RUNTIME PENDING |
| 56 | `56-voice-line-reference-map.md` — **main voice-line reference**: every commentary / pit-radio eid (520) to SDT clip, what the line says, cooldown/blocked, work status; verified gearbox quick map (player 80-93, AI lost 201-206/209, stuck 212-217/211). Generated by `tools/gen_voice_reference.py` from the owner's browse_voices map + EVF-008 tables | GENERATED REFERENCE (owner listening + live event dump) |

## Companion investigation records

- [VBSTRATEGY and modder-claims assessment (2026-10-13)](../investigations/VBSTRATEGY_MODDER_CLAIMS_ASSESSMENT_2026-10-13.md) — modes 0/1/2, geometry pooling, device flags, vertex locks, AMD regression report, DXVK worker distinction and claim-by-claim evidence; no runtime or configuration changes.


- [Plain-language function guide solution assessment (2026-10-01)](../investigations/FUNCTION_GUIDE_SOLUTION_ASSESSMENT_2026-10-01.md) — summary coverage, reproduced stale interpretations, approved-data catalogue and searchable public reader proposal; no public changes made.

- [Corpus GitHub publication (2026-10-01)](../../operations/release_management/CORPUS_GITHUB_PUBLICATION_2026-10-01.md) — upload execution and verification following the owner's clarified public-knowledge/private-implementation boundary.


These records are detailed, evidence-linked internal maps rather than polished
shareable papers. They are preserved under `investigations/` and remain subject to
their stated static/runtime limitations.

- `../investigations/OSC_UI_SCRIPTING_STATIC_MAP_2026-08-30.md` — Options Screen
  Configuration grammar, parser tree, action evaluator, symbol registry, widget
  factory constructor/index boundary, lifecycle, diagnostics, and MAS/resource loading.
- `../investigations/INPUT_FFB_STATIC_RUNTIME_MAP_2026-08-30.md` — DirectInput
  lifecycle, effect creation/download/start, actuator on/off control, action/binding
  surface, analog configuration and smoothing, FFB option families, and the distinct
  unbridged legacy-named event record.
- `../investigations/MULTIPLAYER_NETWORK_STATIC_MAP_2026-08-30.md` — custom Winsock
  topology, browser/host/join states, opcode constructors and packet framing,
  TCP/UDP lifecycle, relay authority, timing synchronization, plaintext trust
  boundaries, and two-peer backlog.

- [GPU execution feasibility assessment (2026-10-13)](../investigations/GPU_EXECUTION_FEASIBILITY_ASSESSMENT_2026-10-13.md) - static CPU/GPU boundary, rewrite scope, prior render plan and profiling gates; no runtime changes.

- [Modern hardware FPS and stability assessment (2026-10-13)](../investigations/MODERN_HARDWARE_FPS_STABILITY_ASSESSMENT_2026-10-13.md) - owner objective, texture/resource investigation, installed EXE LAA flags, misleading legacy names, benchmark and compatibility gates; no engine changes yet.

- [Render Optimizer implementation and native rejection (2026-10-13)](../mods/sdk_sandbox/render_optimizer/README.md) - implemented state-cache prototype, synthetic PASS but test GPU slowdown/access violation; activation hard-disabled, no game changes.

- [Fused vertex packing implementation and native-buffer verification (2026-10-13)](../mods/sdk_sandbox/render_optimizer/VERTEX_PACK_REVIEW_2026-10-13.md) - bit-exact kernel, approximately half CPU update time in test GPU buffer tests; game integration/FPS pending.
