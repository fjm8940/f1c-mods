**Current state (short, maintained): [CURRENT_STATE.md](CURRENT_STATE.md).** Dated entries below are history, newest first.

**2026-10-08 Web Telemetry private compatibility candidate:** the friend's
supplied DLL matches archived v3 `web_telemetry_new.dll`, not v4. The isolated
4.1.0-rc.1 candidate replaces background live-pointer reads with game-thread
mirrors, verifies current Lightloader host/broker contracts, disables unreviewed
mutations, and repairs lifetime/JSON/network/state defects. Focused synthetic
and full SDK gates pass; native track-entry validation and crash attribution
remain pending. [Audit and candidate](../../mods/sdk_sandbox/web_telemetry/README.md).

**2026-09-23 pit voice allowlist correction:** live r13 rejected requested
XPITIN EID 50 before native queue insertion; fuel-clear EID 57 was likewise
absent from the helper's allowlist. `f1c_pit_voice.h` now admits both exact
catalogued player IDs. The r19 client fixture tests the allowlist and queue
retry; full SDK gate passes. Native listening remains pending.
[Live failure and repair](../../../development/mods/pitstop_modding/verification/xpitin-slow-wheel-audio-2026-09-23/REPORT.md).

**2026-09-23 pit crew diagnostic reader:** `f1c_pit_crew.h` bounds the
read-only per-vehicle actor/name/channel walk for the pit-failure trial. The
raw channel ranges are diagnostic, not calibrated sound times. The pit client
fixture and full SDK gate pass; game-side actor tracking remains to be checked.
[Mod evidence](../../../development/mods/pitstop_modding/verification/crew-windows-2026-09-23/REPORT.md).

**2026-09-23 ordinary-race penalty request (experimental):** `f1c_race_penalty_issue.h` wraps the exact-target native stop-and-go/DQ event request separately from the Red Flag restart-penalty hold. It checks the canonical code fingerprint, game thread, scoring-object identity, native penalties setting and event-queue capacity; enqueue is provisional until native scoring confirms a ten-second penalty entry or DQ finish state. Full SDK gate and isolated outside-target refusal pass. The wrapper has not yet been invoked in a native race and is not a general broker capability. [Safety Car candidate and live gate](../../../development/mods/full_safety_car/trials/v0.10.75_player_no_pass/README.md).

**2026-09-23 experimental parked-vehicle clear:** `f1c_vehicle_native_clear.h` guards native `sub_4C6E80` by exact entry bytes, game thread, dormant role and roster identity. The first Safety Car live candidate returned 0 without clearing the roster. A traced `thiscall` proxy subsequently observed one successful native clear per exit and three same-process races without the reproduced crash. This is a private gameplay result, not a general SDK certification or public release. [Live evidence](../../../development/mods/full_safety_car/trials/2026-09-23_session_reentry_crash/LIVE_REPAIR_2026-09-23.md).

**2026-09-22 instant-replay live instances:** `f1c_replay_live_instances.h` scopes identity preservation to synchronous offline race instant replay, around the complete-instruction TwoPathDispatch boundary. Used with the existing per-slot reconciliation gate. [Specification and evidence](../../../development/mods/full_safety_car/trials/v0.10.66_replay_live_field/README.md). Native-byte dispatch/reconciliation tests pass; physics/render acceptance pending.

**2026-09-22 SC replay ownership boundary:** [Implemented contract, native-byte evidence and limits](../../../development/mods/full_safety_car/trials/v0.10.64_replay_ownership/VERIFICATION.md). `f1c_sc_replay_ownership.h` gates the entire owned-SC per-slot reconciliation/name transaction, preserving original identity/count/scene. Exact357-byte fingerprint and7-byte whole-instruction trampoline; canonical SDK gate now executes the original native caller with this guard. Not a gameplay acceptance claim.

**2026-09-22 live replay fields revision 2:** `f1c_live_replay.h` adds the reviewed identity offsets (self +0x4034, roster +0x4894, scene root +0x6F4, name +0x4620) and the fingerprinted native `SetDriverVisibility` (0x4C7AA0) helper used to complete the replay-exit contract. [RF response, SC parked-SC detour and hashes](../../../development/mods/red_flag/trials/2026-09-22/replay-roster-guard/RESPONSE_TO_CODEX_R1_R5.md).

**2026-09-22 reader v13:** fixed read-only native vehicle count, replay mode and104-pointer roster; explicit invalid-slot raw diagnostics; bounded scene traversal only for separately checked, explicit role3 identity, and existing SC status slot/hidden/count fields. [Live SC/replay findings, tests, bounds and restoration](../../../development/mods/red_flag/trials/2026-09-22/massa-stuck-r4/SC_LOCATION_INVESTIGATION.md). 13 reader tests and full SDK gate pass; older pipe-version notes below are historical.

**2026-09-22 live replay roster fields:** `f1c_live_replay.h` declares the reviewed native replay mode global (`0x8AB6E4`, 2 = playback) and the vehicle fields the replay loader itself saves/restores (`+13000` control word, `+4189` visibility, `+18620` bit 2) with the LoadingPhase1 / ProcessDriverEntries / LoadingPhase2 evidence. Used by the RF replay roster guard; the header provides no writes. [Implementation, determination and acceptance checklist](../../../development/mods/red_flag/trials/2026-09-22/replay-roster-guard/README.md).

**2026-09-22 AI pit-strategy restart preservation:** [Reviewed plan block, extent contract, request latch and fail-safe helpers](PIT_STRATEGY_REVIEW_2026-09-22.md). `f1c_pit_strategy.h` declares the count (+0x4588), five 16-byte entries (+0x4590), cursor (+0x45E0) and request latch (+0x454C); capture/restore require a declared extent of at least 0x45E4 and write nothing on an unsupported target. Revision 3 addresses independent review R1-R4 and the r3-review fallback-ownership defect; extracted native and RF adapter tests pass, including the hardened source; native late-race restart acceptance pending.

**2026-09-21 pit retirement voice:** [Existing catalogue and generic AI EID237](PIT_VOICE_REVIEW.md).
Confirmed-garage narration is wired; named-subject/dedup fixtures pass, native listening pending.

**2026-09-21 RF penalty hold review:** `f1c_broker_penalty_hold.h` provides an
optional ABI15 broker capability with Begin/Read/Seal/End. Complete native code
bodies are guarded; a sealed constructor preserves the event ledger across one
session generation while every object is revalidated. New issue events, service
suppression and deadline freezing are tested in `tests/test_broker_penalty_hold.py`.
`f1c_restart_fuel.h` also selects per-model full or final pre-service fuel.
[Implementation, review artifacts and pending native evidence](../../../development/mods/red_flag/trials/2026-09-21/policy-v89/completion-work/penalty-hold/README.md).
Not promoted or installed; isolated tests do not establish gameplay acceptance.

**2026-09-21 restart fuel/native code guards:** [Reviewed fuel leaf and penalty-mode fingerprint](RESTART_FUEL_REVIEW_2026-09-21.md). Actual native fuel bytes pass isolated player/AI tests; full SDK gate passes. RF uses the leaf only in broker callbacks. Full service and post-announcement penalty merging remain unfinished; no gameplay pass or installation.

**2026-09-21 RF restart penalty continuity:** [SDK contract and RF89 implementation record](../../../development/mods/red_flag/trials/2026-09-21/policy-v89/README.md). Inline remaining-lap penalty snapshot tests and full SDK gate pass; broker native service-mode restoration remains a gameplay acceptance gate.

**ESC accelerated-results boundary added:** [v0.10.49 and optional RESULTS_FINALIZE capability](../../../development/mods/full_safety_car/trials/v0.10.49_esc_results/README.md). Full session exit alone missed ESC accelerated completion. New broker and SC installed; automated gates pass, native acceptance pending.

**Optional ABI15 session-end callback:** [SC exit cleanup and native evidence](../../../development/mods/full_safety_car/trials/v0.10.48_session_exit/README.md). Capability-gated callback uses existing RestartGate hook before Game_Post; tests pass, native validation pending.

**Read-only MCP connection:** `python tools/f1c_mod.py memory-mcp start-reader` starts a hidden elevated reader for up to eight hours. [Contract, operation and evidence](F1C_MEMORY_MCP.md). No game writes; freshness is reported explicitly.

**Optional race-control ownership:** [SC/RF acknowledged handoff ABI1](RACE_CONTROL_COORDINATION_2026-09-10.md). Full SDK gate and cross-DLL tests pass; native validation pending.

**Read-only engine motion:** f1c_engine_motion.h observes confirmed EngineRPM zero/positive/unknown only. Test registered in full gate. [Evidence and limits](../../../development/mods/full_safety_car/STALL_CLASSIFICATION_FIXED_2026-09-10.md).

**Native start-stall descriptor:** f1c_start_stall.h is a bounded read-only type5/condition0 observer; not a live engine-status Boolean. test_start_stall.py registered in full gate. [Evidence](../../../development/mods/full_safety_car/START_STALL_FIXED_2026-09-10.md).

**Per-car SC diagnostics:** `python tools/f1c_mod.py safety-car-status --vehicle-slot 20` adds exact-target read-only lifecycle, restart deadline and wheel surface evidence. Requires suitable read privileges; cross-tick sample rejected. [Evidence](../../../development/mods/full_safety_car/MALCHAREK_DEEP_REVIEW_2026-09-10.md).

**Live Safety Car diagnostics:** use `python tools/f1c_mod.py safety-car-status` for read-only ABI1 published trigger gates. Existing mapping only, magic/size/version validated; compare heartbeat for freshness. [Evidence and limitations](../../../development/mods/full_safety_car/AUTO_ACTIVATION_DIAGNOSTICS_2026-09-10.md).

**Native AI restart deadline reader:** f1c_ai_restart_pending.h; read-only simulation deadline, future pending/FLT_MAX sentinel checks. See [implementation evidence](../../../development/mods/full_safety_car/NATIVE_RESTART_EXCLUSION_2026-09-10.md). Registered test_ai_restart_pending.py passes full SDK gate.

Read-only vehicle lifecycle: [reviewed fields, garage geometry and validation limits](VEHICLE_LIFECYCLE_REVIEW_2026-09-08.md). Canonical vehicle-lifecycle-read diagnostic and f1c_vehicle_lifecycle.h; complete SDK gate passes. Cross-track and full-cycle validation pending.

Experimental per-car restart: [exact ABI and validation limits](CAR_RESTART_REVIEW_2026-09-08.md), f1c_car_restart.h. Safety v0.10.4 uses one native reset per return and AI rebind; native repeated-departure/resource safety pending.

Read-only AI interaction accessor: [contract and evidence](AI_INTERACTION_READ_2026-09-08.md), f1c_ai_interaction.h. No new native write or25ms callback. Native traces pending.

Placement API review 2026-09-08: [same-session AI native placement](PLACEMENT_REVIEW_2026-09-08.md), f1c_placement.h. Canonical exe-disassemble --exe PATH --address VA --size N provides bounded read-only exact-target disassembly. SDK/native-wrapper gates pass; v0.10.0 Safety Car installed after game exit; live return validation pending.

Runtime correction 2026-09-08: v0.9.8 local-yellow trial withdrawn after a loading freeze; v0.9.7 restored. The new hook is not runtime-validated despite passing deterministic gates. See [rollback evidence](../../../development/mods/full_safety_car/LOADING_FREEZE_2026-09-08.md).

# Canonical F1C SDK

Execution plan (canonical for LLM-tooling work): [LLM tooling implementation plan](LLM_TOOLING_IMPLEMENTATION_PLAN_2026-09-30.md) — task allocation (DeepSeek / strong model / owner), corrections, acceptance; [DeepSeek handoff D1–D9](DEEPSEEK_HANDOFF_2026-09-30.md). Not started; owner approval pending.

Workflow review: [2026-09-30 Codex review of LLM tooling, SDK quality and static-analysis priorities](LLM_WORKFLOW_REVIEW_CODEX_2026-09-30.md) — evidence-based review and proposed improvements; no runtime or product-stage changes.

Detailed draft: [LLM workflow, tooling and SDK assessment with implementation roadmap](LLM_OPTIMIZATION_ASSESSMENT_AND_ROADMAP_2026-09-30.md) — priorities, proposed contracts, acceptance criteria and targeted-analysis decision rules.

2026-09-08 local-yellow input filter: [reviewed setter ABI and bounds](LOCAL_YELLOW_REVIEW_2026-09-08.md), exposed in f1c_local_yellow.h and catalogue StewardZoneStateSet. Full SDK gate and actual Safety Car predicate/detour tests pass. v0.9.8 installed; native flag/audio gameplay pending.

2026-09-08 ABI14: per-client SetPreload callbacks run synchronously before native DynMan_Init vehicle construction on the reviewed canonical target. Ordinary tasks remain on main updates. Snapshot lifetime, unregister deadlock refusal, old-table capacity negotiation and append-only discovery are covered by tests. Full SDK gate passes; broker and Safety Car v0.9.6 are staged, gameplay pending. See [preload contract/evidence](../../../development/mods/full_safety_car/PRELOAD_FIX_2026-09-08.md).

2026-09-08 broker revision: ABI 13 restart guards are now per client. Every snapshotted guard runs; any non-ALLOW blocks the native mini-restart. NULL clears only the caller's guard. Callback contexts must survive in-flight snapshots until unregister or game-thread synchronization. Full SDK tests pass, including actual setter/dispatch regression. Installed candidate and restoration evidence: `development/mods/full_safety_car/GUARD_CHAIN_FIX_2026-09-08.md` under the workspace root. Runtime coexistence remains pending. Older ABI-13 brokers may still reject a second guard as CAPABILITY_IN_USE.

2026-09-07 Safety Car integration: [reviewed restart ABI and AI input semantics](SAFETY_CAR_INTERFACE_REVIEW_2026-09-07.md) add a no-argument DynMan_Restart signature and classify PassingNorm/TrackAggressionEffective as latched tuning inputs. Lap count remains scoring state. The full test suite now supports `python tools/f1c_mod.py sdk-test --exe PATH` for an explicitly supplied, SHA256-verified canonical executable outside the old installation path. This changes test input selection, not target compatibility. Product build evidence: `development/mods/full_safety_car/LINKED_BUILD_2026-09-07.md` under the F1 workspace root.

`tools/sdk` is the canonical mod-development interface. It merges three evidence
layers without pretending they have equal confidence:

- `name_index.json`: trust-tagged globals and vehicle fields.
- `f1c_sdk_overrides.json`: reviewed, recent facts that have not yet propagated
  into the older indexes, including field width and input/output semantics.
- `mods/sdk/f1c_sdk.json`: historical broad coverage, imported into the JSON as
  `LEGACY` leads only.

Run from the repository root:

```powershell
python tools/f1c_mod.py sdk-info
python tools/f1c_mod.py doctor
python tools/f1c_mod.py sdk-refresh
python tools/f1c_mod.py sdk-test
```

Front-door behaviour for LLM sessions (current as of 2026-09-30; details in
[LLM_OPERATING_CONTRACT.md](LLM_OPERATING_CONTRACT.md)):

- `start`/`sdk-info` report `freshness`, `recent_checkpoint_entries` and ABI
  `conflicts`; the broker ABI is read from `f1c_broker_abi.h`.
- `evidence-query findings TEXT --json` joins curated findings with unreviewed
  `RECORDED` checkpoint summaries.
- `inspect` accepts `0x`/bare/`sub_` addresses and lists navigation-only
  `sdk_links`; `search` uses a ripgrep prefilter with identical results.
- `sdk-test --suite tooling` and `--no-regenerate` are for iteration; only the
  default full gate counts as done. It reports generated-artifact drift.
- `ida-query` answers IDB questions (xrefs, decompile, disasm, bytes) on a
  disposable copy of the typed lab IDB. `ida-export` regenerates the typed
  corpus (`functions_typed_20260930/`, searchable as `typed`).
- History and review: [implementation log](LLM_TOOLING_IMPLEMENTATION_LOG.md).

Generated artifacts:

- `f1c_sdk.json`: machine-readable canonical catalogue. Reviewed ABIs coexist
  with explicitly tagged legacy function and vtable leads.
- `f1c_sdk.h`: C offsets/accessors and reviewed function addresses.
- `f1c_vehicle_struct.h`: padded C/C++ vehicle layout with compile-time offsets.
- `f1c_vehicle_struct_ida.h`: the same layout in IDA-compatible spelling.

Hand-maintained semantic interfaces:

- `f1c_pit.h`: C observations and proven pit commands.
- `f1c_sdk.hpp`: small source-like C++ facade.
- `f1c_runtime.h`: exact-target validation, safe slot access, and explicit
  worker-to-game-thread dispatch queue.
- `f1c_hook.h`: expected-byte, reversible x86 detours that fail closed.
- `f1c_trace.h` and `f1c_trace_schema.h`: allocation-free snapshots of 88
  trusted scalar vehicle fields with typed change callbacks.
- `f1c_reviewed_api.hpp`: generated source-like wrappers for reviewed ABIs,
  explicitly namespaced as game-thread calls. ABI review does not by itself
  make an internal state transition safe as a standalone command.
- `f1c_broker_abi.h`, `f1c_broker.h`, and `f1c_broker.c`: packaged ABI-7
  client surface with an append-only ABI-6 table prefix and load-path-independent
  discovery. Successful discovery pins the selected module; unregister clients,
  stop every caller, then release the API exactly once. Duplicate broker-shaped
  modules fail closed.

Public SDK version: `1.0.0`. The fixed-address target identity is recorded in
`f1c_target.h`, including the executable SHA-256 and PE fingerprint.

## Start a new mod

```powershell
python tools/f1c_mod.py init weather_probe --title "Weather Probe"
python tools/f1c_mod.py pipeline weather_probe
```

All other scripts and batch files are internal implementation details. LLMs
must read [LLM_OPERATING_CONTRACT.md](LLM_OPERATING_CONTRACT.md).

See [NEW_MOD_GUIDE.md](NEW_MOD_GUIDE.md) for runtime threading, guarded hooks,
deployment, unload, testing, and manual production-promotion rules.
See [RECONSTRUCTION_WORKFLOW.md](RECONSTRUCTION_WORKFLOW.md) for turning an
unknown subsystem into a reviewed rewrite boundary.
See [SEMANTIC_MEMORY_LINKER.md](SEMANTIC_MEMORY_LINKER.md) for joining static
object layouts to flat and heap-child runtime captures.
See [VARIANT_RESOLUTION.md](VARIANT_RESOLUTION.md) for memory-map snapshots and
fail-closed address discovery across modified executables.
See [LLM_WORKFLOW_REVIEW_2026-09-30.md](LLM_WORKFLOW_REVIEW_2026-09-30.md) for
measured tool speed/staleness findings and ranked LLM-workflow improvements
(proposals only; nothing implemented).
See [LARGE_MOD_AUTOMATION.md](LARGE_MOD_AUTOMATION.md) for the manifest-driven
multi-subsystem orchestrator, direct raw-source search/inspection, and generated
C++ hook/command artifacts.

## Rules for mods

1. Include the canonical headers; do not repeat a reviewed raw offset locally.
2. Treat fields marked `output` as observations. Request behavior through a
   `command_input` or a reviewed game-thread function instead of forging state.
3. Add new live/static evidence to `f1c_sdk_overrides.json`, regenerate, and add
   a smoke assertion for every width-sensitive field.
4. A function copied from the historical SDK remains a `LEGACY` address until
   its calling convention, stack cleanup, owner object, and thread requirement
   have been reviewed.

`mods/sdk` remains a compatibility input for older mods. New work should not
include it directly.

## Passive runtime validation

Runtime fields can be checked from natural game behavior without writing to the
process. The canonical recipes live in
`analysis/runtime_validation_recipes.json`. For example, start a race and let
several cars cross the line while this runs:

```powershell
python tools/f1c_mod.py runtime-observe lapNumber race_laps_01 --seconds 300 --hz 5
```

This validates the known `veh+0x403C` field. To search the vehicle layout when
the field offset is unknown or suspected to have moved:

```powershell
python tools/f1c_mod.py runtime-observe lapNumber race_laps_scan_01 --mode discover --seconds 300 --hz 5
```

Both modes are read-only. They save the sufficient observation statistics and
a scored report under `runtime_memory/observations/<tag>/`. A saved observation
can be re-scored after behavioral thresholds or scoring are refined:

```powershell
python tools/f1c_mod.py runtime-validate lapNumber runtime_memory/observations/race_laps_01
```

`STRONGLY_SUPPORTED` describes the identity and behavior of a state field. It
does not mean that writing the field is safe; command semantics still require
the natural game writer/call path or a controlled causal test.

The `EngineAngularSpeed` recipe covers `veh+0x39A0`, the engine angular speed
shared by player and AI cars. The runtime layer reports the raw stored value in
rad/s; convert to RPM in the consumer with `rpm = value * 9.549296585513721`
(the same factor as `SpeedMultiplier` at `0x49B8D0`). Live evidence and its
limits: finding `engine-rpm-live-scale-20261009`. The canonical SDK field name
for this offset is `EngineRPM`.

```powershell
python tools/f1c_mod.py runtime-read EngineAngularSpeed --json
```

## Callback recovery extension - 2026-09-08

Optional ABI14 capability and exports are documented in [f1c_broker_recovery.h](f1c_broker_recovery.h). Native-only250ms scoped allowance; normal5ms remains. Game-thread cleanup precedes retirement outside lock with in-flight lifetime retained. Full SDK gate passes. Candidate installed with Safety Car v0.10.5 after game exit; native validation pending. [Product implementation evidence](../../../development/mods/full_safety_car/CALLBACK_RECOVERY_2026-09-08.md).

## Native pit-entry marker - 2026-09-08

[f1c_pit_marker.h](f1c_pit_marker.h) reads the cached Xpitin world marker via a bounded caller reader, with no native function calls. Reviewed Steward_Init/SceneNodePositionGetter layout; full SDK gate passes. Marker geometry/cache availability still requires gameplay validation. [Implementation/evidence](../../../development/mods/full_safety_car/PIT_HANDOVER_2026-09-08.md).

## XPITIN uncached marker correction - 2026-09-08

`python tools/f1c_mod.py pit-marker-read` provides exact-target bounded read-only cache/transform diagnostics. f1c_pit_marker.h derives world point from local+affine transform when native optional cache is empty. Live input resolves52m distance; SDK gate passes. Safety Car v0.10.8 installed after game exit; native validation pending. [Evidence](../../../development/mods/full_safety_car/XPITIN_CACHE_FIX_2026-09-08.md).

Read-only race-data buffer decoder: [field provenance and bounds](RACE_DATA_OBSERVATION_REVIEW.md), used by the standalone [Race Data Collector](../../../development/mods/race_data_collector/README.md). Native acceptance pending.
# Pit voice enqueue addition (2026-09-21)

`f1c_pit_voice.h` provides a bounded native commentary enqueue helper for broker
game-thread callers, with explicit driver subject and native queue acceptance.
[ABI evidence, gates and playback limitations](PIT_VOICE_REVIEW.md).
# Reused pit HUD (2026-09-21)

[AI pit engine restart and native retirement trial](AI_PIT_FAILURE_REVIEW.md)
adds exact-target guarded helpers; native validation is recorded in the pit product.

`f1c_pit_hud.h` ports the original pit-mod vacant-slot HUD writer with bounds
and termination checks. [Evidence and limitations](REUSED_PIT_PRESENTATION.md).
