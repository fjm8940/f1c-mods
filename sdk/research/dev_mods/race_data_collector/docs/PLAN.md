# Definition / specification — 2026-09-14

Owner objective: recreate the complete Java race-data collection role as an independent DLL, review collected/useful data and existing defects, preserve Season Narrative compatibility, and assess a more efficient format.

Acceptance: x86 DLL, no Java dependency, broker discovery/capability/target gates, bounded game-thread copy only; worker-side aggregation and disk writes; valid UTF-8 JSON retaining all legacy top-level sections and consumed field shapes; explicit unavailable data and inference, no invented blame; event/history capture; lifecycle-safe per-race isolation and finalization, retry/collision-safe output; native core tests plus actual Season Narrative parser integration; exact candidate hash and limitations documented.

Format: compatible summary JSON in RaceData for existing imports, append-only JSONL journal in separate recovery subdirectory for observations/events. Binary formats would require consumer changes and obscure manual review; compact JSON eliminates whitespace without changing schema. Journal and incomplete outputs must not be auto-imported as race results.

Scope: development/mods/race_data_collector; additive reviewed SDK observation surface if required; component and governance indexes with before-state preservation. No installation, publication, gameplay writes, JAR/consumer mutation, or production edits. No structural moves. F1CUI-derived gameplay features such as VSC/DRS remain independently owned; collector must report unavailable rather than invent events when no provider exists.

Risks: snapshots before all cars finish, session resets and slot reuse, player/spectator confusion, poll gaps, partial frames, asynchronous stop/unload, write failure, stale target offsets, incorrect raw units, import assumptions. Abort unsafe reads on target/capability/session mismatch; record loss and incompleteness. Validation uses synthetic deterministic frames and consumer imports without touching season databases; installed gameplay validation remains a separate gate. Preserve all failed outputs.

Documentation destination: this component's README/status, audit, coverage table and evidence. Baseline validator has 10 existing errors; continuing isolated work under renewed explicit owner request, with no stage success inferred from those errors.
