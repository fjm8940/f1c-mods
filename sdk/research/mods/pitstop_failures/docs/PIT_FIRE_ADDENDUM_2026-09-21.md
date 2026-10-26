# Rare pit fire linked to fuel-rig failure

Owner request, 2026-09-21: allow a rare pit fire when the fuel pump has problems
and the problem is called out. This supersedes the earlier plan's blanket exclusion
of fire as a future feature. It does not waive review, game-thread ownership,
preservation or runtime validation. DeepSeek is currently repairing R1-R6; this
addendum is a separate follow-on task, not a request to interrupt those repairs.

Status: investigated and specified; not implemented or live-tested here.
Canonical product remains development/mods/pitstop_modding, development prototype.
No policy source, config, SDK source, existing plan or review was edited.

## Existing implementation found

`f1c_ida_decompiled/mods/sdk_sandbox/fuel_fire_lab/` contains prior implementation:

- fuel_fire_lab.c: flame/smoke particles, body anchors, expanding ground pool,
  fire_start/fire_stop, timed ignition in armed_pit_tick and a pit hold.
- fuel_fire_lab.ini: chance_pct=0 (manual-only), burn_seconds=25,
  engine_kill=1, dnf_after_burn=0; flame appearance/spread parameters.
- README.md and generated manifests: historical lab guidance and context.

Canonical CLI route classifies it supported-sandbox; evidence-query findings
fuel_fire_lab returns three CONFIRMED historical records. This supports reuse of
the prior feature investigation; it does not prove modern broker compliance.
The README is stale: it says25% automatic default/player-only, while current INI
sets0% and source supports target-relative visual emission on AI slots. Engine
bank/ignition and retirement operations still contain player-only paths. Do not
infer AI effect safety from a README or from targeting code alone.

The current source calls ParticleTrailEmitter, modifies shared per-track effect
descriptors and engine fields, and runs a CreateThread worker. fire_start can zero
engine angular velocity/RPM when engine_kill is enabled. fire_stop zeroes a player
flame bank and restores shared descriptor snapshots. This is not isolated ownership
and is not suitable as-is for the new recoverable event.

`python tools/f1c_mod.py sdk-query function ParticleTrailEmitter --json` and
`inspect ParticleTrailEmitter --json` report trust LEGACY and prototype null.
The raw decompiler exposes the call shape, but ABI/game-thread approval remains
unestablished. The raw emitter consumes the game's random table/index too: new
visual emissions can advance native RNG, so isolate the policy's RNG and measure
native side effects rather than claiming visuals are completely inert.
No reviewed high-level owned fire API was identified by this focused search.
This is not proof that no such API exists anywhere; Phase G must inventory current
broker capabilities before creating a duplicate. No runtime calls were executed.

## Callout mapping

Verified in the historical full_event_dump.json catalogue:

| Audience | Event ID | Clip | Catalogue metadata |
|---|---:|---|---|
| Player fuel hose/pump problem | 56 | PWS_2001_79.mp2 | score54, SDT55, priority40, cooldown1, not blocked |
| AI fuel hose problem | 185 | PWS_2001_132.mp | score188, SDT197, priority20, cooldown1, not blocked |

Legacy src/pit_commentary.c names56 EID_P_FUELHOSE and sends it on first detected
fuel-ramp activity; fuel_eid maps all fuel variants to that event. Do not confuse
event ID with score or SDT index. No new fire-specific spoken line was verified.
Do not advertise a spoken 'fire' callout unless its clip is located and listened
to. Use the fuel-problem audio and an explicit FIRE IN THE PITS HUD indication.
AI radio needs verified subject-driver substitution, not a player clip.
Catalogue mapping/source use is verified; actual playback was not tested today.

## Proposed gameplay policy

Fire is a rare complication of an already selected FUEL_RIG fault. It is not a
new independent top-level roll, engine-failure event or generic effect of a bad stop.
Model the rare complication as a leak/spill at the refuelling connection associated
with the fuel-rig fault; ordinary low pump flow alone is not asserted to ignite.

Provisional default: fire_given_fuel_fault_bp=500 (5% of fuel faults), configurable
0..1000 for normal tuning; forced test mode is separate. The owner's requested
feature is confirmed; this exact number is proposed tuning, not historical data.

At the tension preset fuel risk90bp=0.9%, conditional5% gives absolute0.045%
per fully eligible stop (about1 in2222). With44 independent fully eligible stops,
P(at least one fire)=1-(1-.00045)^44, approximately1.96% per race across the field.
Under all pressure flags, fuel risk149bp gives0.0745% per stop. Pressure affects
the chance of a fuel fault once; do not multiply fire severity a second time.
These are policy-selection rates. Runtime prerequisites can reduce realised fires.

Keep the current top-level fault/clean probabilities unchanged. Once FUEL_RIG is
selected, use a dedicated PIT_FIRE RNG domain and one conditional draw. Ordinary
fuel faults keep their existing subtype selection/distribution. If fire is chosen,
record a fire complication on that parent plan; do not roll again on each tick,
retry, audio event or ignition attempt. The recovered seeded API must be used.
Product of bp probabilities has finer resolution than1bp; do not round0.045% to
an independent1bp bucket or change category odds to fit it.

First implementation target: recoverable fire, no extra DNF probability. A visible
fire must NOT implicitly zero engine fields, forge retirement or use lab defaults.
Wheel terminal probability remains unchanged. Fire damage/terminal fire may be
designed later with its own explicit total terminal budget and native API review.

Proposed duration tuning: ignition1-2 simulated seconds after the fuel-problem
announcement, burning triangular(4,7,12) seconds, then inspection triangular(2,4,7)
seconds. These are bounded gameplay values, not copied from the25-second lab.
No infinite burn option. Reuse wheelgun/pump animation only where validated; no
fabricated extinguisher animation claim. Prefer a small localised filler/sidepod
fire with limited ground spill to a full-car inferno for every recoverable case.

## Event ordering and ownership contract

1. Commit service and decide base fault/complication once, freezing identity/seed.
2. At actual refuelling service readiness, activate the fuel problem. Acquire
   required owned hold and fuel-service suspension capability before visual fire.
3. Publish FUEL_PROBLEM_STARTED with stable service/event ID. Queue the correct
   fuel callout and display the pump-problem HUD. Playback confirmation, if the
   broker exposes it, is distinct from successful enqueue.
4. Schedule one ignition from that announcement milestone. If audio is muted,
   absent or queued behind another line, an acknowledged HUD announcement can
   satisfy presentation; never wait indefinitely or make another random decision.
   If neither can be presented within a bounded window, cancel the fire branch
   to an ordinary pump fault and log FIRE_SUPPRESSED_PRESENTATION.
5. Recheck same epoch/vehicle/service, still stationary/in-service, refuelling
   allowed/requested, required capabilities acquired and valid effect handle.
   On failure suppress fire without reroll or surprise ignition at the next stop.
6. Emit IGNITE once, then bounded controlled emission via reviewed game-thread
   provider. Keep car in box and fuel delivery suspended through owned commands.
7. Emit EXTINGUISH once, stop emission, allow bounded particles to dissipate,
   inspect, resume remaining service, then release owned hold. No premature
   recovered/GO cue from the parent pump fault while the fire is active.

Pending/base fuel resolution is subordinated to the fire transaction. When a fire
starts it replaces the ordinary pump recovery schedule; do not emit two recovered
events or sum arbitrary full delays. Compute final completion from the suspension,
remaining jobs, extinguishing and inspection timeline, respecting overlap. If
normal job baselines/fuel accounting are unknown, runtime integration remains
blocked; do not invent a deadline or silently keep refuelling through a fire.

Cancel on premature service exit, explicit cancel, discontinuity, reset or unload.
Late callbacks cannot ignite another service. Pause freezes scheduled simulation
progression. Cleanup is idempotent, releases only owned resources and cannot reset
someone else's flame bank or descriptor state. Multiple cars can have distinct
handles. A capacity conflict suppresses one fire explicitly, not one car's fuel
service or another effect's ownership. Record selected/suppressed/ignited/resolved
counts separately; no covert redraw to achieve target observed frequency.

## SDK integration work required after R1-R6 acceptance

Inventory current broker first. Proposed provider operations, NOT existing API names:
begin_pit_fire(service_identity,preset,lifetime)->owned_handle;
update/stop(handle); owned hold acquire/release; fuel-service suspend/resume;
HUD/audio enqueue and optional playback observation. Typed status/error results
must distinguish unavailable/invalid/conflict from success. All calls on the
reviewed game thread, with bounded work and negotiated capabilities.

Promote the useful lab rendering mechanism behind that provider: validate exact
variant/calling convention, descriptor allocation/lifetime and emitter budget.
Prefer owned instances; do not blindly copy a descriptor containing renderer
pointers or overwrite the shared per-track descriptor and restore stale bytes.
Engine bank3, engine_kill, DNF hotkeys and the lab's independent automatic roll
are excluded from this integration. Do not load both as competing auto-failure
owners. Do not drive the legacy shared-memory GUI as a production bridge.

No low-level implementation is claimed by this document. If a needed ownership
capability is absent, make the smallest reviewed SDK addition with focused tests
and its mandatory checkpoint/sdk-test requirements. Guarded runtime and human
visual/audio validation remain required before installation/release approval.

## Follow-on implementation and acceptance checklist

After the other agent finishes R1-R6 and that repair passes review:

- Extend pure policy/plan with the conditional complication and independent RNG
  domain; update strict versioned config and docs. Omitted fire setting defaults
  off for old schemas; a newly selected fire-enabled preset explicitly sets500bp.
  Do not silently change existing user tuning or conflate schema versions.
- Exhaust all10000 conditional draws:500 fire/9500 ordinary fuel faults with the
  test preset; none for clean/tyre/stall/terminal faults. Zero setting disables it.
- Verify fuel masking/refuelling disabled/unknown identity cannot select fire;
  unchanged baseline category, corner and non-fire subtype/duration RNG streams.
- Independent large-sample expected absolute rate = p_fuel*p_fire_conditional;
  record denominator and count parent fuel faults, not all stops as conditional.
- Fake adapter replays parent announcement, audio busy/muted, HUD fallback,
  presentation timeout, capability failure, late tick, small output buffers,
  duplicate events, pause/time reversal, cancel/unload and simultaneous cars.
- Exactly one ignition/extinguish/recovery per realised fire; no parent recovery
  before clearance, no extra DNF, no new epoch used to hide table exhaustion.
- SDK provider tests verify cancellation, resource ownership, bounded emission,
  no shared-descriptor corruption and no other vehicle engine/fuel modification.
- Guarded game tests verify chosen target visually, callout subject/content/order,
  stopped-car hold, actual fuel suspension/conservation, resume, frame cost,
  particle expiry, next stop, track change, reset and conflict with other mods.

## Investigation evidence and restoration

Sources read: lab README/C/INI/manifest; SDK query and inspect; historical event
catalogue56/185; development fuel callout implementation. Reference hashes and
captured catalogue/CLI results reside in verification/pit-fire-investigation-2026-09-21.
No live capture occurred, so no current-game freshness or validation is claimed.
The original SDK material remains unchanged, including contradictory historical
README claims. No scripts from the lab were executed, built or staged.

This addendum and an index link are the only maintained-document additions.
README before-state: workspace_organization_audit/PITSTOP_FIRE_DESIGN_BEFORE_2026-09-21/README.md.
Restore only after preserving then-current state; retain new artifacts. Source,
configuration, runtime state and governance status untouched. Eight pre-existing
workspace-validator errors remain. Wasted effort: broad fire searches produced
noise/truncation; Windows glob syntax needed correction; no builds/tests/polling.
Next gate: finish and review current policy repairs, then this separate policy and
SDK-provider extension. Do not interpret this addendum as accepting A-F repairs.
