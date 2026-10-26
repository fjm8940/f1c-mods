# 51 — Static-analysis system priorities: where will mapping add the most modding value?

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)
**Date:** 2026-08-30 · **Author:** Codex · **Status:** STATIC ONLY

## Abstract (TLDR)

This assessment prioritizes under-mapped game systems that can be advanced without
running the game. The best immediate target is the camera system: it has a broad,
coherent static footprint, human-visible modding value, bounded configuration
formats, and relatively little dependence on timing-sensitive runtime behavior.

The recommended static-analysis sequence is:

1. camera architecture and configuration;
2. AIW/navigation consumers and limits;
3. championship/career persistence;
4. audio routing and configuration;
5. OSC/telemetry exposure;
6. replay/VCR persistence.

Input/force-feedback semantics, weather behavior, live observation commands, and
multiplayer packet modification remain valuable, but their important claims depend
too heavily on runtime state or interoperability to make them the best use of a
static-only period.

## 1. Introduction

The research question is which incompletely mapped F1 Challenge systems offer the
best future-mod value while runtime analysis is unavailable. Scope includes binary
surface area, configuration consumers, likely user-facing leverage, and how much of
each system can be established statically. This is a prioritization document, not a
claim that every named system is fully understood or ready for implementation.

## 2. Method: static analysis

The canonical decompilation/research corpus was queried through the supported SDK
front door for subsystem names and related symbols. Results were compared with the
existing paper index and component documentation to estimate mapping coverage. The
search returned strong candidate clusters for AIW (21 hits), OSC (24), Sound (25),
Replay (25), NetComm (25), Camera (25), Championship (25), and VCR (16). Hit counts
are discovery signals only: substring collisions and generic names prevent treating
them as measurements of subsystem size. **DERIVED**

No generated knowledge graph was available at either the workspace root or the SDK
repository during this assessment. Conclusions therefore came from the canonical
source/research corpus rather than graph nodes or inferred graph relationships.
**FACT**

## 3. Method: runtime verification

PENDING. Runtime access is currently unavailable. Each target must eventually be
checked with a narrowly scoped experiment that verifies configuration loading,
state transitions, visible behavior, and any proposed hard limit before release.

## 4. Results

### 4.1 Priority table

| Rank | System | Static suitability | Prospective mod value | Basis and next deliverable |
|---:|---|---|---|---|
| 1 | Camera | High | High | Coherent managers, view families, configuration keys, fixed layouts, and renderer handoff can be mapped statically. Produce an architecture/configuration contract. **DERIVED** |
| 2 | AIW/navigation | High | Very high | File consumers and bounded path structures are accessible statically; better mapping could support track tooling, AI lines, pit behavior, and validation. Produce a consumer/limit map. **DERIVED** |
| 3 | Championship/career persistence | High | Medium-high | Save/load paths, season progression, and serialized structures are suited to cross-reference analysis. Produce a schema and lifecycle map. **DERIVED** |
| 4 | Audio | Medium-high | Medium-high | Configuration ingestion, channel ownership, and event dispatch can be mapped, though perceptual tuning remains runtime work. Produce routing/configuration documentation. **DERIVED** |
| 5 | OSC/telemetry | Medium-high | High | Exportable state is strategically useful for overlays, analytics, and external tools; field meaning and cadence require later live validation. Produce a field provenance map. **DERIVED** |
| 6 | Replay/VCR | Medium | Medium-high | Persistence and playback functions are visible, but timing, compatibility, and corruption behavior require runtime evidence. Produce a file/lifecycle boundary map. **DERIVED** |

### 4.2 Targets deferred during static-only work

- Observation commands and general live-state tooling depend on proving reads and
  transitions against a running game. **DERIVED**
- Input and force-feedback semantics depend on device behavior, sign, scale, and
  timing that static code alone cannot settle. **DERIVED**
- Weather is visible statically, but its gameplay and presentation semantics are
  predominantly runtime questions. **DERIVED**
- Multiplayer packet modifications have high compatibility and safety costs; static
  layouts alone do not establish peer behavior or failure modes. **DERIVED**

### 4.3 Work selected from this assessment

Camera mapping was selected first. Its completed static deliverable is
`50-camera-system-static-architecture.md`, covering the manager lifecycle, local,
static and tracking families, configuration slots, state machines, shared view
layout, renderer boundary, hard limits, and runtime backlog. **FACT**

## 5. Discussion

This ranking optimizes work that can reach a useful, bounded static result now; it
is not a permanent statement of product importance. AIW/navigation may ultimately
offer greater ecosystem value than cameras, but camera analysis had the cleaner
initial boundary and faster path to a reliable contract. Search counts are not
coverage metrics and should never be used alone to declare a system mapped.

The missing knowledge graph reduced navigation convenience, not evidentiary quality:
the underlying source and maintained research documents remain authoritative. A
future graph may accelerate cross-component discovery, but graph-derived edges must
still be verified against source before becoming evidence.

## 6. Conclusion

During a static-only period, map cameras first and AIW/navigation second. Continue
with championship persistence, audio, OSC, and replay when those two deliverables
are bounded. Defer claims whose useful meaning depends on live timing, devices,
weather transitions, or network peers.

## Appendix A — Corrections to prior notes/tools

None. This paper records the comparative assessment that previously existed only in
the 2026-08-30 working session.

## References

- Game binary and canonical decompilation corpus for `F1 Challenge Online.exe`
- `docs/README.md` — existing research coverage index
- `50-camera-system-static-architecture.md` — first selected deliverable

---
*Changelog: 2026-08-30 — initial static-only prioritization and methodology record.*

## 2026-08-30 static-gap review addendum

This follow-up rechecked the shortlist against the completed 7,132-function
recovery index, the generated configuration schema, source-module string
anchors, and focused raw-function inspection. It changes the recommended order
for the next static-only work and corrects one misleading label in the original
assessment. No runtime evidence was collected. **DERIVED**

### Correction: `OSC` is not telemetry

The `OSC` hits belong to the front-end/UI resource script parser. The inspected
entry point at `0x004944A0` opens script resources, loads MAS-backed assets and
intro/legal media, then dispatches the parser at `0x00492160`; its diagnostics
include `Log\\osc.information` and `Log\\osc.err`. These references do not
establish Open Sound Control or a network telemetry protocol. **DERIVED**

F1C does have a separate native telemetry recorder. `TelemetryDataDump`
(`0x004D4A80`) opens an output file, writes a 57-channel header, embeds scene and
vehicle metadata, and establishes a statically visible sampling range of 1–40
Hz. Its per-frame producer path is separately named
`PositionUpdateWithTelemetryRecording`. This is a file-format/provenance target,
not an OSC target. **DERIVED**

### Revised quick-win order

| Rank | Static target | Why it is bounded now | Recommended deliverable |
|---:|---|---|---|
| 1 | Native telemetry recorder | A small named cluster exposes session initialization, a 57-channel header, file output, sampling bounds, and the position/update producer. Runtime is needed for exact cadence and consumer compatibility, not for the structural map. **DERIVED** | File layout, channel provenance, writer lifecycle, options, and later runtime checks. |
| 2 | Championship/career persistence | The recovery index has 41 championship/career-named functions; 29 are outside the current coarse subsystem tags. Dedicated readers/writers and standings/season loaders are present, and the generated schema already identifies career state fields. **DERIVED** | Save schema, read/write lifecycle, standings update graph, and corruption/version boundaries. |
| 3 | OSC UI/resource scripting | The parser, diagnostics, MAS/resource loading, and front-end caller boundary are visible statically. The name is misleading, but the script system itself is a useful modding surface. **DERIVED** | Grammar/command inventory, resource resolution, screen lifecycle, limits, and error behavior. |
| 4 | SpecialFX/material reactions | `specialfx.cpp` anchors ten named functions and a complete `Enter -> Setup -> Init -> Restart -> Post -> Exit` lifecycle. The generated material/reaction parser has 48 extracted fields, although 39 proximity matches are suspect and require raw-code review. **DERIVED** | Lifecycle plus manually verified material/reaction configuration contract. |
| 5 | AIW/navigation | File-path construction, scene loading, waypoint debug rendering, spatial-grid queries, and AI consumers are visible. It remains high-value but crosses more ownership boundaries than the targets above. **DERIVED** | AIW consumer/limit map split into file loading, waypoint structures, spatial lookup, racing-line consumers, and pit-lane consumers. |
| 6 | Replay/VCR | Config keys, recording UI/state paths, and the replay load state machine are visible, including file staging and player-vehicle restoration. Backend names such as `AudioStreamOpen` are generic legacy aliases and need manual call-site review. **DERIVED** | Record/load lifecycle and file-boundary map before attempting a full binary schema. |
| 7 | Audio routing/configuration | Audio already has 145 functions tagged by the recovery index and a clear subsystem lifecycle, so the gap is reviewed contracts rather than discovery. The whole subsystem is too broad for a single quick pass. **DERIVED** | Narrow maps for event routing, commentary, ambient/vehicle emitters, and configuration ingestion. |
| 8 | MAS/archive and scene resource loading | File/version checks, archive helpers, scene loaders, and path resolution are statically discoverable and largely deterministic. **DERIVED** | Read-only archive/lookup ownership map and failure contract. |

### Not quick static wins

- A general network telemetry/OSC export surface is not currently evidenced.
  Do not design one from the UI script engine's `OSC` name. **DERIVED**
- Multiplayer packet modification still requires peer/interoperability testing,
  even though NetComm dispatch and socket infrastructure are statically broad.
  **DERIVED**
- Input/force-feedback sign, scale, device behavior, and timing remain runtime
  questions. Static work can map lifecycle and configuration only. **DERIVED**
- Weather configuration can be catalogued statically, but player-visible weather
  behavior and transitions are not a quick static proof. **DERIVED**
- Camera is no longer an unmapped candidate at the architectural level: paper 50
  supplies the static map, with runtime semantics still pending. **FACT**

### Evidence and limitations

- `shadow_source/architecture/CURRENT_SCOPE.md` records 7,132/7,132 function IR
  coverage. This is breadth coverage, not reviewed subsystem semantics. **FACT**
- `analysis/recovery_index.json` currently provides only six coarse tags (`ai`,
  `vehicle`, `pit`, `session`, `audio`, and `render`), so an untagged function is
  a map-navigation gap rather than proof of an unknown behavior. **FACT**
- `.understand-anything` contains an incomplete scan workspace but no assembled
  `knowledge-graph.json` or `meta.json`; the paper therefore continues to rely on
  the canonical recovery index and raw/derived function evidence. **FACT**

*Addendum changelog: 2026-08-30 — corrected OSC/telemetry conflation and ranked
the next bounded static-only mapping targets.*
