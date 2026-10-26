# Feasibility: modern-language rewrite of F1 Challenge '99-'02 as open source (2026-10-06)

Stage: orient -> investigate. No code or product changed. Not legal advice;
legal points are INFERENCE unless marked FACT and must be confirmed by a lawyer
before any public step.

## Verdict

- Technically feasible as a multi-year effort. Workspace already holds most of
  the knowledge needed.
- Publishing a rewrite derived from the decompile is NOT feasible legally.
- Feasible path: clean-room open-source engine that loads the player's own
  legally owned game data (OpenMW / OpenRCT2 / OpenTTD model), new name, no
  "F1"/FIA/EA/ISI marks, no shipped game assets.

## Workspace evidence (FACT, checked 2026-10-06)

| Asset | State |
|---|---|
| `f1c_ida_decompiled/functions/` | 7,143 decompiled functions of the game EXE |
| `f1c_ida_decompiled/shadow_source/ir/functions/` | 7,132 LLM semantic IR files (~99.8%). Confidence fields: semantics HIGH 4,457 / MEDIUM 2,483 / LOW 143 (approx, sampled per-file field); owner mostly LOW. Archived; worker agreement is not trust. |
| `f1c_ida_decompiled/replacement_engine/` | Paused archive. C++/CMake 32-bit bootstrap: config read + install inventory only, no sim or render (~40 non-blank .cpp lines). Route says do not extend from SDK work. |
| `f1c_ida_decompiled/findings.jsonl` | 816 entries; no prior rewrite/open-source feasibility finding. |
| `f1c_ida_decompiled/docs/` | Subsystem architecture docs (camera, AIW, AI brain, pit, championship, mechanical failures). |

## Legal blockers (INFERENCE)

1. Copyright: the game is proprietary (ISI developer, EA publisher). Code
   translated from the decompile or `shadow_source` is very likely a derivative
   work. Precedent: re3/reVC (GTA III/VC decompilation-based rewrites) received
   a DMCA takedown and lawsuit from Take-Two (2021).
2. Trademarks: "F1", "Formula 1", "F1 Challenge" belong to Formula One
   Group / EA. Project needs a neutral name.
3. Assets: cars, tracks, sounds, textures, liveries, driver names are not
   redistributable. Mod-community content carries its own author permissions.
4. EULA reverse-engineering clauses: jurisdiction dependent (EU interoperability
   exception is narrow).
5. The workspace itself is tainted for clean room: anyone who read the
   decompile cannot write the open code under a strict clean-room claim.

## Clean-room route (recommended if pursued)

- Team A (may read decompile): writes behavior specs only — file formats (MAS,
  GMT, AIW, HDV, GDB, VEH), rules, AI behavior, physics observables, measured
  timings. Existing `docs/` are a starting point but must be scrubbed of
  addresses/pseudocode before handoff.
- Team B (never saw decompile): implements from specs. Agents used for Team B
  need fresh context with no workspace access.
- Engine loads data from user's own install; ships zero original assets.
- Language: C++20 or Rust; renderer via SDL3 + Vulkan/bgfx; data-driven.
- Validation: side-by-side lap time / AI behavior comparisons against original
  via `f1c-memory` telemetry (observation, not code copying).

## Effort estimate (INFERENCE)

Hardest parts: ISI tire/physics model fidelity, AI driving and AIW navigation,
multiplayer. Comparable open reimplementations (OpenMW, OpenRCT2) took many
contributors and many years. Solo + LLM: playable single-car-on-track prototype
plausible in months; parity with original handling and AI is years.

## Cheaper alternatives

- Keep improving the original via mods/SDK (current objective, already legal
  status quo for the community).
- Open-source only original tooling (Lightloader, SDK headers without game
  code, file-format readers written fresh).
- Publish file-format specifications.

## Decisions needed from Matias

- Pursue at all vs stay on mod SDK (current active objective).
- Legal consultation before any public repo.
- Accept clean-room team split (Matias has read the decompile, so cannot implement under a clean-room claim).

## Pending validation

All legal claims unverified by counsel. Effort numbers are estimates.
Workspace validator already reports pre-existing error `Unexpected top-level
directory: .claude` (not caused by this task).

Wasted-effort check: `coverage_report.py` failed on a stale hard-coded docs
path (`%USERPROFILE%/Desktop/f1c_ida_decompiled/docs/...`); otherwise none observed.

## Addendum: Bevy as engine candidate (2026-10-06)

Source: https://bevy.org/ (read 2026-10-06). FACT from site: Rust, ECS,
MIT/Apache-2.0, 3D renderer (wgpu-based, backend agnostic), glTF loading,
audio, UI, hot reload, Windows/Linux/macOS/Web.

Assessment (INFERENCE):
- Good fit: licence compatible with open source; ECS suits 20-car sim
  (one entity per car, systems for physics/AI/timing); Rust safety.
- Gaps: no built-in vehicle/tyre physics (write own fixed-step sim, or
  Avian/Rapier only for collisions); no loaders for ISI formats (MAS, GMT,
  AIW, HDV, GDB, VEH) - write as standalone Rust crates (convert GMT to
  meshes at load or offline to glTF); pre-1.0 with breaking API changes
  each release (pin versions); racing wheel/force-feedback input not
  built in (needs extra crate, e.g. SDL bindings).
- Suggested shape: engine-independent `sim` crate (deterministic physics,
  rules, AI, fixed tick) + `formats` crates + thin Bevy app for render,
  audio, UI, input. Keeps sim testable headless and engine swappable.
- Clean-room rule unchanged: Bevy choice does not reduce legal blockers.

## Addendum: token budget estimate (2026-10-06)

Measured (FACT): decompiled `functions/` = 7.9 MB text (~2.3M tokens at
~3.5 chars/token); `shadow_source/ir/functions/` = 27.3 MB (~7.8M tokens).
No per-wave token totals were found in shadow_source reports.

Estimate (INFERENCE; agentic loops re-read context, assume ~5-10k total
tokens per finished, tested line of code, mostly cached input):

| Phase | Output size | Total tokens |
|---|---|---|
| Specs (Team A) | ~0.3-0.6M tokens of specs | 20-60M |
| Format readers + Bevy viewer | ~5-10k LOC | 30-100M |
| Drivable prototype (1 car, physics, timing) | ~30k LOC | 150-300M |
| Parity (AI, rules, 20 cars, career, UI) | ~100-150k LOC | 0.5-1.5B |

Driver of cost is input re-read, not output. Mitigations: small crates with
tight specs, headless tests, fresh short sessions per task, cheap model for
mechanical work with Claude review (existing DeepSeek delegation pattern).
Physics/AI fidelity tuning is owner play-testing time, not tokens.

## Addendum: project started (2026-10-06)

Owner approved start. Separate workspace `%USERPROFILE%\Desktop\F1C_OpenEngine`
(git repo) with read-only registry `sources.toml` + bridge `tools/f1src.py`
into this workspace. Claude = coordinator/spec writer/QA, DeepSeek = clean-room
implementer. Findings (FACT, observation): F1C models are .MTS (49,283 entries),
no GMT; MAS layout verified on 685 archives / 75,578 entries
(spec: F1C_OpenEngine/specs/MAS_FORMAT.md). Header `data_size` unreliable
(111/685 mismatch). Stub left at development/open_engine/README.md.
