# Silverstone fixes — 0.2.2

Owner authorized fixes for the native-clock/pit-duration bug, excessive tyre events and misleading missing-lap history. Canonical development component remains verify/candidate. No SC/RF, broker or game-memory writes introduced.

## Implementation

Clocks >=100,000,000 seconds enter a separate native-finalization observation path. Final classification snapshots still update, but race clock remains the last observed racing time. No ordinary lap crossing, pit exit, consumption, contact, speed or tyre event is generated from those snapshots. Open pit visits produce an incomplete-visit event rather than an invented duration. Finalization clock is retained explicitly as diagnostic metadata, not race elapsed time. Non-boundary finalization waits two monotonic seconds with all observed competitors terminal; existing boundary completion and RF veto remain. Returning to normal time starts a new capture.

Raw tyre condition values now emit an initial baseline, then cumulative changes >=0.01 no more often than every five simulation seconds; changes >=0.25 are immediate. Final raw snapshots and tyre compound changes remain. No puncture or failure meaning is inferred from a continuous value. Small variations below thresholds intentionally do not form an event history.

Capture metadata and Narrative prompts explicitly distinguish final native classification from observed race history. Simulated lap totals are not presented as observed laps. No algorithm can recover gameplay that was never sampled; full lap history remains incomplete when native results advance the field after live observation.

## Evidence

[Candidate](../trials/build_20260914_224502_310/): DLL SHA256 `edfc4d7bbc0b3f4a037fbda80a5f0fe4de2eecb35a74792ea050d03c829e1e5e`.

Component build/core/DLL-negative-host/strict-JSON/actual Narrative tests pass. New core regression uses Silverstone's pit entry113.0787165 and finalization1,000,000,000.1476024; classification completes without any invented pit completion or lap record, and observed race time stays113.0787165. Smooth tyre drift is suppressed; a large raw jump emits immediately.

[Recorded tyre replay](../trials/build_20260914_224502_310/silverstone-replay.log): all8957 tyre events from the real Silverstone JSON replayed through the actual C++ collector produce92 samples (98.9729% reduction). This is recorded-value replay, not a new live race or full-frame reconstruction. Test source is tests/silverstone_tyre_replay.cpp. Existing source/SDK input manifest matches the candidate; additional replay/repair tools are separately preserved. SDK unchanged; no unnecessary full SDK rebuild for these collector-only fixes.

Historical latest Silverstone export repaired in place with complete original preserved and SHA256 recorded in [repair manifest](../trials/build_20260914_224502_310/silverstone-repair-manifest.json). Original producer version retained; metadata.repair records the correction instead of impersonating a new capture. Classification is byte-value-equivalent after parsing. Removed8865 tiny tyre events,51 non-racing-time events and one invalid completed pit visit. Retained an incomplete pit observation. Filtered unobserved finalization lap/position/weather event histories. Earlier sequence IDs retained, not renumbered; gaps are documented. Exact last live snapshot cannot be recovered, so historical last observed time is explicitly an event-time bound. No missing race events fabricated. Original journals untouched.

[Repaired Narrative test](../trials/build_20260914_224502_310/repaired-narrative-test.log) passes with explicit native-finalization warnings and reports only under the isolated trial. Installed season database unchanged.

## Installation and remaining gate

At preparation time the game PID24152 remains running; replacement DLL and updated bridge await normal game exit. Owner was asked to close it. No hot unload/injection or process termination. Candidate is ready for exact-hash upgrade via tools/upgrade_local.py once closed. A later installation manifest in the candidate directory is authoritative if present. New-build advancing gameplay verification remains pending.

Restoration: historical JSON original location is in the repair manifest; copy back only after preserving any later edit. Maintained documentation before-states under workspace_organization_audit/RACE_DATA_FIXES_BEFORE_20260914_224732. Installed upgrade tooling preserves prior DLL/bridge before any replacement. No deletion or public promotion.

Documentation gate: linked from README/status, test/replay/repair evidence exists. Baseline validator has ten unrelated errors. Wasted-effort check: no material repeated large reading, unnecessary broad gate, failed/retried command or idle polling; build and recorded replay were necessary validation, game exit is the only installation blocker.
