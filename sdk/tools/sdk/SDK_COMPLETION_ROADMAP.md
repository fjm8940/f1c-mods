# SDK completion roadmap

Date: 2026-08-06

Goal: make the supported F1 Challenge executable broadly programmable through
stable, documented modding interfaces without requiring mods to patch raw game
addresses independently.

Delegation, worker validation, and phase execution are defined in
`SDK_DELEGATED_EXECUTION_PLAN.md`.

“Bulletproof” below means resistant to bad mods, unload/reload, session changes,
hook conflicts, and supported executable variants. It does not mean portable to
an unrelated executable without fresh validation.

## Current position

- Reverse-engineering index: complete for 7,132 discovered functions.
- Reviewed public ABI wrappers: 22.
- Typed vehicle fields: 101.
- Internal game-update anchor, bounded dispatch, single-owner broker, hostile
  client controls, one transactional semantic command, same-process restart,
  second-session generation rejection, interactive garage, and natural
  full-weekend exit: runtime observed on the exercised single-player path.
- Broker ABI 6 capability discovery, generation-safe vehicle handles,
  immutable vehicle snapshots, bounded lifecycle/vehicle event polling, and
  fail-closed stale-session/stale-object rejection: runtime observed.
- Canonically packaged discovery plus ABI 7 session/environment snapshots:
  canonical regression and live startup/retrieval/orderly-unload validated.
- Estimated completion toward the full goal: about 20%.

## Required work

| Priority | Deliverable | Done when | Future-proof confidence after completion |
|---|---|---|---|
| 1 | Session lifecycle model | Garage, loading, race start, restart, exit, and a second session are observed in one process; stale objects and tokens are rejected | **High (90%)** for the supported EXE |
| 2 | Broker discovery and ABI negotiation | Mods locate one broker without fixed DLL assumptions, negotiate versions/capabilities, and fail closed on incompatibility | **High (90%)** |
| 3 | Stable object identity | Vehicle/session handles use generation checks instead of retaining raw pointers; slot reuse and teardown tests pass | **High (90%)** |
| 4 | Public event system | Documented bounded events cover session, simulation tick, vehicle creation/destruction, lap, pit, damage, and race control | **High (85–90%)** |
| 5 | Typed observation API | Read-only snapshots expose validated vehicle, session, timing, weather, rules, track, camera, and input state | **High (90%)**; safest large capability gain |
| 6 | Semantic command library | Reviewed commands cover AI/pit control, damage/failure, race control, weather, vehicle placement, camera, UI, audio, and input | **Medium-high (80–90%)** per command; each requires its own runtime proof |
| 7 | Transaction and rollback layer | Multi-field changes validate preconditions, execute on the game thread, and rollback on failure or mod unload | **High (85–90%)** |
| 8 | Hook/resource ownership | Central registries arbitrate code hooks, file/archive lookup, textures, models, sounds, shaders, and UI assets | **Medium-high (80–85%)**; engine internals remain the main risk |
| 9 | Overlay and custom UI SDK | Mods can render panels, HUD elements, debug tools, and receive normalized input without patching renderer internals | **Medium-high (80–85%)** |
| 10 | Content and data extension | Schemas and interceptors safely add/replace cars, tracks, rules, championships, physics data, audio, and visual assets | **Medium (70–85%)**; hard-coded engine limits must be discovered individually |
| 11 | Multiplayer boundary | Packet observation and explicitly validated server/client commands are separated from unsafe raw networking hooks | **Medium (70–80%)**; protocol and trust risks cannot be eliminated completely |
| 12 | Diagnostics and containment | Per-mod logging, timing budgets, crash attribution, conflict reports, capability permissions, and quarantine work in live tests | **High (85–90%)** for detectable failures |
| 13 | Executable variant resolver | Every address/offset is selected through fingerprints and invariants; known variants have regression fixtures; unknown builds refuse mutation | **High (90%)** for registered variants, **low** for unknown EXEs until mapped |
| 14 | Mod packaging and tooling | Templates, manifests, dependency/version resolution, build/deploy tools, configuration, examples, and hot-reload policy are documented | **High (90–95%)** |
| 15 | Regression laboratory | Automated static tests plus repeatable live weekend/session/teardown scenarios cover every promoted event and command | **High (90%)**, limited by what can be automated in the original game |
| 16 | Security and expert escape hatch | Safe APIs are the default; raw memory/call/hook access is isolated, opt-in, version-locked, and visibly marked unsupported | **High** for the safe tier; **intentionally low** for expert raw access |
| 17 | Public SDK v1 freeze | ABI compatibility policy, semantic versioning, migration guide, complete reference, sample mods, and release bundle pass acceptance tests | **High (90%)** for declared v1 guarantees |

## Promotion rule for every capability

A capability is not finished merely because an address or function name exists.
It must have:

1. instruction-level ABI and data-width review;
2. explicit lifecycle/thread/precondition rules;
3. fail-closed target and object validation;
4. bounded live proof through the broker or another owned boundary;
5. unload, conflict, stale-handle, and second-session tests;
6. a regression fixture, documentation, and an isolated example mod.

## Realistic guarantee

The finished safe SDK can be made highly reliable for the exact supported EXE
and explicitly mapped variants. It cannot guarantee that every imaginable game
behavior is reachable: hard-coded limits, closed renderer/physics internals,
third-party plugins, and unknown executable changes may still require new
reverse engineering. The expert tier can preserve near-raw-source freedom, but
that tier cannot simultaneously be made bulletproof.

## Immediate sequence

1. Preserve lifecycle behavior as automated regression scenarios; replay and
   multiplayer lifecycle remain separate future evidence gates.
2. Package process-local broker discovery and complete the old/new/missing/
   duplicate live compatibility matrix; an isolated candidate already exists.
3. Expand snapshots from vehicles and core session/environment state to timing,
   rules, track identity, camera, and input.
4. Extend polling events with reviewed lap, pit, damage, failure, and
   race-control transitions.
5. Promote semantic commands one subsystem at a time under the rule above.
