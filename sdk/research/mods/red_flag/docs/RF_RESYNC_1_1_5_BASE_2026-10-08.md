# Red Flag — re-sync `src/gr44.c` to the shipped 1.1.5 base (2026-10-08)

Status: **DONE (re-sync only). STOP for owner approval before 1.1.6.**
Owner decision 2026-10-08: option 1 — re-sync `src/gr44.c` to the shipped 1.1.5
base, prove the pipeline reproduces the shipped hardened source and DLL, then
update the harden PIN with a recorded diff review.

Follows `docs/RF_SOURCE_CHECK_2026-10-08.md` (the stale-source STOP).

## What changed

`src/gr44.c` was stale (it predated v1.1.4-rc.4). Four items were ported from the
shipped `releases/v1.1.5-rc.2/redflag.hardened.c` into the canonical dev source:

1. `#include <tlhelp32.h>` (module scan support).
2. Loader host exclusivity gate: `RFP_MIN_GENERATION 1u` ->
   `RFP_HUB_2_GENERATION 2u`; new `RFP_CAP_HUB_2`; `rf_loader_exclusivity_matches`
   now requires `(RFP_CAP_EXCLUSIVE_HOST | RFP_CAP_HUB_2)` and
   `compatibility_generation >= RFP_HUB_2_GENERATION`.
3. New `rf_find_broker_module()` (Toolhelp scan for the one loaded module
   exporting `F1CBrokerGetInfo`); `rf_broker_bind()` and
   `rf_broker_bind_after_dependency_wait()` call it instead of
   `GetModuleHandleA("f1c_game_update_broker.dll")`.
4. `RF_HOST_GATE_TEST` early-return provided inside `rfe_1d3b7a91`
   (`rf_secure_loader_start`).

No other source change. Protection boundary untouched: `harden_release.py`
`TARGETS` unchanged; the only TARGET whose body grew is
`rf_secure_loader_start` (item 4), exactly as the shipped hardened source
already encodes.

## Evidence (fact)

| Artifact | Before | After |
|---|---|---|
| `src/gr44.c` | `7411BE16…817` | (edited; see `generated/redflag.c` hash) |
| `generated/redflag.c` | `871DA8A7…698` (old PIN) | `420D160A73E1BEDA6F1B598B80CC26E171B6DF5ACE2B8DB079C6E84EA04AF652` |
| `harden_release.py` PIN | `871DA8A7…698` | `420D160A…652` |

Reproduction of the shipped base:

- `make_release.py(src/gr44.c)` -> `generated/redflag.c` = `420D160A…652`
  (sandbox-verified equal to the on-disk file).
- `harden_release.py(generated/redflag.c)` -> content **identical** to
  `releases/v1.1.5-rc.2/redflag.hardened.c` (`A0C471BB…`): 11,875
  byte-segments, **0 content differences** (line-based diff).
- Byte difference is only the line terminator on 10 lines: the 10 lines added or
  replaced by items 2 and 4 are LF-only in the shipped patch product and CRLF in
  our regeneration (2 `#define`s, 2 lines of `rf_loader_exclusivity_matches`, 4
  host-gate lines, and their continuation lines). C ignores the terminator, so
  the build is unaffected.
- Built with the release-profile flags (`build.ps1` equivalent) the regenerated
  hardened source produces `redflag.dll`
  `43050C2DFD607EC4EC2C0702C2096261EA903B1EE78D0C18EB504DD6D0940EEC` — the
  **exact shipped/live catalog 1.1.5 DLL**.

## Diff review for the PIN update (the 4 items only)

- Item 2 touches `rf_loader_exclusivity_matches` (forced-inline protection
  helper, not a TARGET) and two `#define`s.
- Item 4 touches the `rf_secure_loader_start` TARGET body (adds the
  `RF_HOST_GATE_TEST` return); the hardened transform rewrites its `return
  TRUE;` into the `rf_protected_result`/`rf_protected_exit` form, which the
  shipped hardened source contains and this regeneration reproduces.
- Items 1 and 3 are outside the protected set.
- No TARGET added, removed or reordered. Event-code map regenerates
  deterministically (crate `generated/redflag.c.private.json`/output sidecar from
  the regeneration).

## Preservation

- `src/gr44.c.BEFORE_20261008_acRFresync`
- `generated/redflag.c.BEFORE_20261008_acRFresync`
- `tools/harden_release.py.BEFORE_20261008_acRFresync`
- `workspace_organization_audit/RF_RESYNC_BEFORE_20261008/gr44.c`

## Pending

- Owner approval to implement Red Flag 1.1.6 on this base (rf_usage.h on
  `f1c_usage.h`; log tap; shared VEH install-after-init / remove-on-unload;
  session-end summary via the broker + host-unload flush; version 1.1.6;
  `build.ps1` `-I mods/sdk`; `test_usage.py`; regenerate + harden; meta minimum
  covering the used broker capabilities). Not started.

## Wasted-effort check

- Repeated reading: the 11 k-line source was read only in the four target
  regions and the re-sync diff.
- Over-broad builds: one baseline and one re-sync DLL build, each needed to
  compare exact hashes.
- Failed commands: none this step.
- Waiting/polling: none.
