# Mod SDK expansion mission

Date: 2026-08-06. Active priority: make the existing game broadly programmable
through safe, source-like modding interfaces. Full replacement-engine recreation
is paused; `replacement_engine/` remains an isolated bootstrap and is not the
current delivery target.

## Desired capability

Approach the practical freedom of source access while retaining the original
executable: typed state inspection, lifecycle events, reversible hooks, reviewed
game-thread commands, resource interception/replacement, subsystem facades,
runtime diagnostics, mod conflict detection, and repeatable vanilla-versus-mod
regressions. Raw addresses alone do not satisfy this goal.

## Promotion ladder

1. Use the complete 7,132-function recovery index to select high-leverage
   subsystem boundaries.
2. Generate three independent static candidates per function with bounded
3. Validate schema, cited evidence, numeric grounding, and dependency cuts;
   reconcile disagreements without promoting model confidence as fact.
4. Instruction-review ABI, owner object, stack cleanup, field widths, and hook
   boundaries. Add only reviewed functions to `f1c_reviewed_api.hpp`.
5. Establish thread/lifecycle requirements and capture natural runtime evidence.
   ABI correctness alone never authorizes arbitrary invocation.
6. Expose the narrowest stable interface: observation, event, command, resource,
   or explicitly unsafe expert hook. Prefer semantic facades over original
   internal function granularity.
7. Build isolated example mods and differential regressions, verify unload and
   hook coexistence, then promote manually from `mods/sdk_sandbox`.

## Expansion tracks

- Core lifecycle: game tick, session/race phases, dispatch, event bus.
- Vehicle control: physics inputs, damage, AI, rules, weather and track state.
- Presentation: camera, renderer, HUD/text, UI layout and custom overlays.
- Interaction: keyboard/wheel/controller mapping and command injection.
- Media/content: audio events, archive/resource lookup, asset overrides.
- Multiplayer: packet observation, validated message APIs and server lifecycle.
- Tooling: typed memory browser, trace correlation, hook broker/conflict map,
  crash diagnostics, symbol variants, hot-reload-safe configuration and docs.

## Active closure objective

The `sdk_expansion_r1_high_leverage` and `sdk_expansion_r1_closure_core` static
waves have completed candidate validation, deterministic reconciliation, and
instruction review. No public interface was promoted from either wave.

`0x0042C690` now has one bounded active-race runtime observation. It used one
thread and one opaque receiver throughout 21,209 calls, but ran at 265.45 calls
per second while advancing the fixed simulation clock at 400.027 ticks per
session-second. It is therefore an observed internal variable-batch update
anchor, not a once-per-frame public event.

The isolated dispatch proof passed 256/256 persistent worker-posted no-op tasks
with zero missing, duplicate, or wrong-thread executions. The hook and queue
drain reported the same thread and detached cleanly.

The canonical queue now has a strict bounded-drain API and a deterministic
wraparound/full-queue/wrong-thread smoke test. This closes the standalone queue
budget defect, but not broker ownership or live hostile-producer integration.

The single-owner broker proof also passed: two clients executed 512/512 tasks
with zero missing, duplicate, or wrong-thread execution; callback self-unregister
was refused, normal unregister and stale-token rejection worked, and a second
broker image failed closed against the already-owned entry bytes.

The hardened broker stress proof passed: a hostile producer completed 14,040
tasks while a polite producer completed 40/40 with sub-10 ms maximum latency.
A deliberately slow 10 ms callback executed once, then its client was
quarantined and seven remaining tasks were purged. Startup phase 0 and active
phase 5 were both observed, and the repaired unload path detached cleanly.

The first broker-owned semantic command is now observed. ABI 3 copies a vehicle
slot and boolean for reviewed `SetCallToBox` (`0x0042AC70`), validates phase and
AI liveness on the game thread, and retains no client pointer. A paired 1 then 0
test executed twice and restored the original value before gameplay observed
the transient state; invalid value, player slot, and stale token gates passed.

ABI 4 now adds a transactional CallToBox pulse. On AI slot 1 it applied 1 for
one original game update, then restored the exact prior 0 after return. Static
consumer review shows `AIRacingLine` (`0x00425920`) and path refresh
(`0x00426430`) branch on that field. The live proof completed one pulse and one
restore with no game overwrite, then unregistered and detached cleanly. This is
a bounded consumer-visible execution window, not a claim that one update is
long enough to produce a visible pit-stop commitment.

ABI 5 now owns an exact-byte RestartGate observation hook and derives a session
generation from its return episodes. A game-owned mini-restart request advanced
generation 1 to 2 in the same Windows process, with one observed exit, one
re-entry, one return-4 boundary, and 6,183 subsequent main-update calls in the
second active session. An old-generation request was rejected and both hooks
detached cleanly. Six DeepSeek Flash/xhigh lifecycle candidates validated; their
remaining semantic disputes were not promoted as facts.

The active objective is garage/full return-to-menu evidence plus packaged
broker discovery and ABI negotiation. Arbitrary
native callbacks are not the intended public surface. Do not dispatch another
broad static wave and do not increase the public ABI count until those gates
close.

Evidence:
`shadow_source/reconstruction/reports/main_game_update_runtime_observation_20260806.md`
`shadow_source/reconstruction/reports/main_game_dispatch_runtime_proof_20260806.md`
`shadow_source/reconstruction/reports/game_update_hook_broker_runtime_proof_20260806.md`
`shadow_source/reconstruction/reports/game_update_broker_stress_runtime_proof_20260806.md`
`shadow_source/reconstruction/reports/broker_set_call_to_box_runtime_proof_20260806.md`
`shadow_source/reconstruction/reports/broker_call_to_box_transactional_pulse_runtime_proof_20260806.md`
`shadow_source/reconstruction/reports/sdk_lifecycle_r1_static_review_20260806.md`
`shadow_source/reconstruction/reports/broker_same_process_session_restart_runtime_proof_20260806.md`

Live machine-readable progress and the next safe objective are recorded in
`tools/sdk/AUTOMATION_CHECKPOINT.json`. Automation runs should inspect this
checkpoint after mandatory repository instructions and update it only on a real
state transition.
