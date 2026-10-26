# AIW initialization-window feasibility — 2026-08-30

## Scope and outcome

Read-only orientation for a possible future investigation of (a) the exact AIW
file opened during track initialization and (b) malformed-record and
high-index behaviour.  No game was launched, restarted, sent input, attached
to, or modified.  No game asset, executable, production artifact, or existing
evidence artifact was changed.

**Outcome: blocked.**  No approved disposable development game installation or
disposable AIW track asset was identified.  The supported SDK can observe a
running process and the persistent parsed database, but it has no current
front-door capability that captures the short-lived file handle inside the
AIW-initialization window.

## Evidence and current capability

- `python tools/f1c_mod.py start --json` reports the SDK in
  `BROKER_DETERMINISTIC_REGRESSIONS_GATED`; its current priorities do not
  include AIW mutation or initialization-handle capture.
- `python tools/f1c_mod.py doctor --json` passes but reports the SDK target as
  optional/not present.  The SDK automation checkpoint also records
  `game_processes: 0` at its last update.
- The supported read-only front-door command
  `python tools/f1c_mod.py runtime-read RECIPE --recipes PATH` can use the
  preserved custom recipes in
  `runtime_memory/observations/aiw_navigation_20260830_live_header_recipes.json`
  and `runtime_memory/observations/aiw_navigation_20260830_live_vehicle_waypoint_recipes.json`.
  They expose the persistent selected-path buffer, validated header fields,
  waypoint allocation pointer/count, and per-vehicle waypoint pointers.
- The prior AIW report identifies the persistent parsed path as a strong
  candidate identity, not a captured open handle.  Its lifecycle review says
  that the loader opens the AIW into a local initialization handle, passes it
  to the parser, and closes it before initialization returns.  A steady race
  therefore has no current AIW handle to inspect.
- Historical `CreateFileA` hook work exists only in preserved investigations;
  it is not a supported front-door observer and is not authorization to
  attach, inject, or repeat a hook experiment.

## Installation and asset boundary

- The workspace validator reports both root game paths absent:
  `%USERPROFILE%\Desktop\F1\F1 Challenge V10` and
  `%USERPROFILE%\Desktop\F1\F1 Challenge V10_clean`.
- A prior read-only artifact names a non-workspace game at
  `%USERPROFILE%\Documents\F1 Challenge V10` and its `2002_Monza.AIW`
  candidate.  Existence was checked read-only only.  Neither the artifact nor
  workspace governance labels it as a disposable development installation;
  it must not be used for malformed/high-index tests without explicit owner
  approval.
- `docs/52-aiw-navigation-static-architecture.md` explicitly requires a
  separately approved disposable development asset before causal mutation,
  unauthored-capacity, or malformed-input testing.

## Why malformed/high-index testing is unsafe without that approval

Static/cross-track evidence establishes these hazards, not safe behaviour:

- The waypoint parser writes current-record fields before its `wp_ptrs`
  count check; excess textual records can write past the allocation before the
  guard is reached.
- Link remapping rejects `-1` and values at/above the declared count, but
  other negative values are not visibly rejected before pointer formation.
- Grid records between authored rows and `startinggrid` capacity are
  zero-filled but in-bounds; access at or above the allocation is the actual
  unchecked-risk boundary.
- No maximum waypoint-count or allocation-multiplication-overflow guard has
  been recovered.

Accordingly, neither a malformed record nor a high index may be introduced
into a shared or non-approved track, and no conclusion about controlled
failure versus corruption is currently supported.

## Safe execution specification (requires owner approval first)

1. **Authorize and identify the disposable target.**  Record an owner-approved
   development-only game root and a copied, uniquely named AIW asset outside
   production/live installations.  Record pre-test SHA-256 hashes and a
   restoration map.  Do not use the root paths currently reported missing or
   the historical Documents installation by inference.
2. **Add a reviewed front-door observer before the run.**  Extend only
   `python tools/f1c_mod.py` plus its contract/tests with a passive,
   exact-target-validated initialization-window observer.  It may report the
   normalized opened path/identity and close timing, but must neither hold the
   handle nor alter execution.  It must fail closed on an unexpected process,
   module hash, path outside the approved root, duplicate process, or any
   write-capable operation.
3. **Validate without mutation.**  In a human-authorized guarded full weekend
   (never QuickRace), start the observer before a natural track initialization.
   Capture only the handle/path identity and selected-path/database snapshot;
   stop after the parser closes the handle.  Hash and retain an additive log.
4. **Specify a separate mutation experiment.**  Do not combine it with handle
   capture.  Define one bounded change per copied asset, expected parser
   result, exact count/index range, watchdog/abort criteria, and restoration
   procedure.  Abort immediately on unexpected file writes, malformed counts,
   process instability, effects outside the approved root, or loss of target
   identity.
5. **Human validation and restoration.**  A human verifies gameplay and the
   copied asset's post-test hash/restoration.  Preserve every candidate and
   observation; do not promote any result from a crash or apparent success.

## Commands and validation performed

Read-only front-door checks: `start --json`, `sdk-info --json`, `doctor
--json`, `capabilities --json`, `recommend`, `route`, `evidence-query`,
`sdk-query`, and command help for runtime observation/correlation/snapshot
interfaces.  Root workspace validation was also run; it reported pre-existing
missing game-root paths and four unrelated source-like production files.

No stage change is justified.  Pending owner validation: disposable target
designation and the reviewed, front-door initialization-window observer.
