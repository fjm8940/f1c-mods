# Pit test panel — offline bridge 0.3.0-testbridge.1

Run `launch-test-gui.ps1` from this component. It builds and checks a fresh
development DLL, then opens the panel. Select Player or an AI, press **Force
100% failure + request pit-in**, and watch the fault, normal/actual stop times
and timeline. **Request a normal stop** compares against ordinary probability.
Closing the panel stops its owned backend. Reopening starts a fresh test.

This is an **offline test**, with 24 explicitly virtual drivers. It does not
control the running game, play game audio, ignite fire, suspend native fuel,
or retire/teleport a car. `launch-test-gui.ps1 -Live` retains the future native
adapter connection and remains disabled when that adapter is unavailable.
The user's request to finish and test authorizes continued implementation;
missing native capabilities, rather than another permission round, block the
in-game acceptance target.

## What is connected

Tk panel -> loopback HTTP -> validated one-shot receipt -> real policy/session
-> actual policy plan and semantic commands -> repaired offline C transaction
provider -> confirmed modeled release -> displayed timeline.

Only the top-level failure chance changes. The development selector draws from
the sum of the existing eligible category weights, without changing their ratios.
It consumes the ordinary initial category draw before subselection, preserving
corner/subtype RNG position and the independent duration stream. Normal mode
uses the original selection. No seed search, changed identity or fixed outcome.
An eligible problem may overlap another job and add no total stop time.

The accepted v2 source files remain byte-identical. `tools/build_test_bridge.py`
checks their exact hashes and applies small exact-match transformations into a
fresh trial directory: explicit force input at plan selection and session commit,
zero-eligible refusal before reservation, and normal API wrappers passing false.
It fails on parent drift rather than guessing a patch. All timing, recovery,
queue/history/cancellation logic comes from that source. Trial manifests identify
generated sources and DLLs. This development derivative is not a promoted policy
version or a runtime broker ABI.

The bridge supplies synthetic normal jobs: tyres 8s, fuel 10s, restart 1s.
It models ordinary added delay through the repaired C transaction provider;
it does not use the mock's fire pathway. A fresh provider owns each active
service, avoiding the mock's eight-lifetime-slot limit. The real session retains
completed/cancelled token history across those providers. At most 24 concurrent
cars; at most 256 retained command receipts; capacity refusal is explicit.
Terminal selections stay visible as **retirement unavailable**, without claiming
retirement, garage transfer, audio or Go. Restart the offline panel after such a
blocked terminal scenario. Fire is still a separate unimplemented policy extension.

## Transport and lifecycle

Offline server uses 127.0.0.1:18744, distinct from native port 18743. Host and
origin checks, JSON content type, strict duplicate/nonfinite rejection, 4096-byte
request bound and socket timeout restrict the endpoint. Commands validate full
instance/session/object identity and availability before reservation. Exactly
one reservation precedes simulated call-to-box acceptance; refusal unwinds it.
Request IDs deduplicate and reject changed payloads. Only normal and forced
one-stop operations exist; no user-set durations, categories or job baselines.

The forced command is the existing TEST_GUI_CONTRACT command. Offline comparison
uses operation `normal_pit` and failure_probability_bp=0 to mean **no override**,
not zero probability. It follows the same plan/session/provider code.
Receipts remain for the instance lifetime; expiry uses monotonic wall time and
is checked before a stop commits. Clock discontinuity cancels arms and disables
the backend. Closing this owned offline backend cancels outstanding requests;
the future external native adapter retains its own lifetime contract.

Status explicitly identifies `mode=offline`, virtual names, modeled time and
events. It does not refresh stale native data because it reads no native data.
The GUI does not retry uncertain POSTs; authoritative receipts resolve them.
Logs under each trial capture seed, DLL hash, commands, plans and semantic events.

## Verification and next gate

Run `python -B tools/verify_test_gui.py`. It builds both architectures, runs
200,000 forced decisions per architecture with weighted-rate/stream checks,
once-only/history/clock tests, existing policy regressions on the derivative,
Python backend/HTTP/GUI tests and the real Tk disconnected smoke test.
All evidence goes to fresh trials; nothing is deleted.

Actual connected GUI interactions and final verification are recorded in
`verification/gui-bridge-2026-09-21/REPORT.md`. The native acceptance target in
GAMEPLAY_ACCEPTANCE_2026-09-21.md remains pending: service identity/readiness,
native work baselines, owned hold, presentation/audio, fuel/effect controls,
retirement and guarded clean-install gameplay must still be implemented and
validated. This panel does not bypass that missing implementation.
