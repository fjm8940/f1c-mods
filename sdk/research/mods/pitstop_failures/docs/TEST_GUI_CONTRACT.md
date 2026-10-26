# Development test GUI and adapter contract — 2026-09-21

Update: [offline real-policy bridge and testing instructions](GUI_TESTING_2026-09-21.md)
are now implemented. Default launcher starts the explicitly offline backend on
18744; `-Live` retains native port 18743. Native runtime connection remains
unimplemented. The original native contract and initial frontend evidence below
are preserved as context; the earlier default-launch/disconnected description is
superseded only for the offline mode.

Status: GUI implemented; runtime connection NOT IMPLEMENTED.. The panel cannot currently force an in-game failure or request a pit stop. It stays disconnected with its action disabled until a compatible adapter is present. This is a concrete UI and integration contract, not evidence that the complete package works.

Owner request: select the player or any AI driver, make a failure 100% likely and request that driver's pit-in, changing only probability. Implemented UI: tools/pit_test_gui.py; launch with `powershell -NoProfile -ExecutionPolicy Bypass -File launch-test-gui.ps1` from this component, or run the Python file directly. Python with Tk is required (verified locally: Tk 8.6). No additional packages.

## Intended behavior

Select a current human/AI driver and press **Force 100% failure + request pit-in**. This submits a single development command. It must arm exactly one future eligible service and request call-to-box for the same full vehicle identity. It does not force physical entry to the box or change strategy/fuel/tyre work. A call-to-box acknowledgement means the semantic request was accepted, not that the driver has reached the pits.

Only the top-level chance of a failure changes to 100%. Conditional selection among eligible failure categories keeps their existing relative weights, and all subtype, duration, recovery, overlap, ownership and cleanup paths remain normal. The conditional fire chance remains unchanged: this button does not guarantee a fire. Unsupported categories still follow the normal adapter's documented eligibility/suppression behavior; do not silently invent native retirement/effects. If there is no eligible failure, refuse/retain until eligibility rather than pretending a failure occurred.

This is a one-stop override with a 300-second command lifetime, cleared on session/object replacement. Normal INI files and global probabilities are untouched. Production parser caps must not be relaxed to accept 100%: provide an explicit development-only selection input at the same decision boundary, then execute the ordinary plan/session/provider path. Keep it off in production. Do not search seeds or change identities to obtain a failure; domain streams other than the top-level selection stay unchanged.

## Proposed transport (adapter implementation required)

Local HTTP at 127.0.0.1:18743. The GUI never follows redirects or environment proxies. The future development adapter owns the endpoint; if the port is occupied by an incompatible process the GUI refuses its protocol. Do not expose it remotely. Keep request processing bounded; validate content type/body size/origin and reject browser cross-origin access. Only explicit development test mode enables mutation. Background network handling queues semantic work to the broker's game thread; it must not call game code directly. Implement through the canonical SDK workflow, not an independent injector or legacy memory writer.

`GET /v1/status` returns:

```json
{
  "protocol": "pit-test-v1",
  "test_mode": true,
  "instance_id": "unique-per-adapter-start",
  "session_generation": "17",
  "observed_unix_seconds": 1790000000.0,
  "session_active": true,
  "force_and_pit_supported": true,
  "drivers": [
    {"slot": 0, "object_generation": "8", "name": "Driver name",
     "role": "human", "retired": false}
  ],
  "requests": {}
}
```

The example is a schema fixture, not live data. Generations are decimal strings. Roles: human/ai/remote; remote and retired cars are excluded. observed_unix_seconds is the wall-clock publication time of the real snapshot; it is not the simulation clock and must not be refreshed over stale game data. The GUI rejects data older than three seconds. Backend must independently validate freshness and the current session/object identity when applying a command, not trust UI validation. instance_id changes on adapter restart.

`POST /v1/commands` receives:

```json
{
  "protocol": "pit-test-v1",
  "request_id": "uuid",
  "instance_id": "unique-per-adapter-start",
  "session_generation": "17",
  "slot": 0,
  "object_generation": "8",
  "operation": "force_failure_and_pit",
  "failure_probability_bp": 10000,
  "scope": "next_eligible_service",
  "ttl_seconds": 300
}
```

Backend transaction: validate capability/session/identity and existing pending override; reserve the one-shot override first; submit the broker call-to-box request; if that request is refused, remove the unconsumed override and mark rejected. A failure to reserve must never request the pit stop. Define broker queue acceptance versus execution failure in receipts; revalidate on execution and cancel the override if the pit request fails. Never report success merely because a JSON command arrived.

Deduplicate by (instance_id, request_id). On timeout the GUI does not resend or create another request. GET status carries receipts keyed by request_id, with `state` and `message`. States: accepted, armed, consumed, rejected, expired, cancelled. Consumed means the normal policy committed the selected driver's eligible stop using the override—not that the stop/fire succeeded. Receipts must be retained for at least the instance lifetime in bounded storage; reject new requests when full rather than evict unresolved receipt identities. Closing the GUI does not revoke a request; it expires or is consumed on the backend. One outstanding request per panel is intentional. The GUI waits for a terminal receipt before another submission.

Backend must expire an unused arm after 300 wall-clock seconds, and cancel it on session/object changes, invalid identity, unload or authoritative time discontinuity. This timeout only bounds test arming, not an in-progress failure; once consumed, normal simulation-time lifecycle governs the stop. A pending arm must not attach to a service already committed. Reopening the GUI cannot authorize another backend override for a vehicle already armed; return an explicit refusal.

## Required adapter tests before enabling the button

- Real roster, matching player/AI identities and slot reuse refusal.
- Arm+pit acceptance and every partial failure; no orphan arm or unrequested pit-in.
- 100% top-level gate on eligible stops, ordinary conditional weights and untouched duration/subtype/fire streams; no change to global config or other cars.
- One-shot consumption, duplicate HTTP request, response loss, reconnect, expiry, reset/unload and already-committed service.
- Unsupported/missing native providers reported truthfully; no fake successful fire/retirement.
- End-to-end guarded gameplay: call-in, actual service readiness, normal policy plan, presentation, hold, recovery, cancellation. This cannot run until DS's runtime adapter/providers exist.

The user's GUI request authorizes building this test control. It does not constitute evidence that the pending native mechanisms are verified. Do not install an unreviewed runtime merely to make the GUI appear connected.

## Current evidence and restoration

Six focused Python tests pass: exact 10000bp command for player/AI, stale/future/nonfinite data refusal, disabled capability refusal, retired/remote filtering, slot reuse and duplicate identity refusal. Tk smoke test opens the real window and confirms the disconnected action is disabled. Commands: `python -B -m unittest discover -s tools -p test_pit_test_gui.py -v` and `python -B tools/pit_test_gui.py --smoke-test`.

No connected backend or in-game test was performed. No policy/SDK/game/config file changed. New files: GUI, tests, launcher, this contract, and verification/test-gui-2026-09-21 evidence. README has an index link; its complete before-state/restoration map is workspace_organization_audit/PITSTOP_TEST_GUI_BEFORE_2026-09-21. Preserve new files; restore README by copy only if needed. Product remains implement/prototype; no release or pipeline promotion.

Documentation gate: this contract is indexed by README, distinguishes completed frontend checks from pending backend/game work, and specifies DS follow-up. Workspace validator stays at eight known baseline errors, zero warnings. Wasted-effort check: none observed for repeated large reads, broad tests, failed/retried commands or polling/idle time.
