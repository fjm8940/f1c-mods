# Multiplayer Overhaul latest-release check - 2026-10-14

Product: coop_mode / Multiplayer Overhaul. Canonical development: development/mods/coop_mode. Lifecycle: shipped, observe, production maturity; next gate learn. Objective: check latest release identity, distribution integrity, automated checks and pending validation. No deployment, installation, runtime change or gameplay test authorised by this check.

## Verified facts
- Live HTTPS catalog still serves Multiplayer Overhaul 1.0.1. Catalog SHA256: E49383DB54B8ECDA20BDA8BC86DADDA22396AE4DEFE3167E5410B5A6DAC1A704.
- Public ZIP: 196015 bytes; SHA256 E9C71B0123C73978CD5CABE49CDF990879F25C80223BE7DC60E416E48614841F. Matches local production ZIP and recorded release.
- DLL read directly from public ZIP: B4073BFC5F5B87ECE332906338CC18596BB6D78F2E760E5D4B1EF930CD500097. Matches catalog and release record.
- Release source snapshot sets RequireMod and RequireModCoop to zero at initialisation and after configuration load. The release record describes this as disabling host player removal after modded clients were removed when the side channel was unavailable.
- `python -m pytest development/mods/coop_mode/tests -q -p no:cacheprovider`: 16 passed. This does not execute every script-based check.
- Each tests/test_*.py also invoked directly with `python -X utf8`: eight scripts exited 0 (address inventory, bridge ABI, menu logic, trust, N2 protocol, node claim gating, node policy, target probes). test_usage.py exited 1 with four envelope failures. It hardcodes 1.0.0 at lines 125 and 147 while both development and release-snapshot coop_usage.h define 1.0.1. Other reported usage privacy/counter/size checks passed. Interpretation: stale test version assertions explain these failures; no gameplay conclusion follows.
- Workspace validator before this check: two errors, unexpected top-level .claude and plans. Neither directory changed.

## Recorded evidence and unresolved reports
- [1.0.1 release record](../releases/v1.0.1/RELEASE_RECORD.md): shipped hotfix, exact binary has not been run in game according to the record.
- [Tester incident investigation](BUG_WRONG_SEASON_CONTENT_2026-10-14.md): wrong-season (2012 versus 2000) download and teleporting AI remain open. The investigation ranks Classes/folder selection as likely content causes after log evidence weakened the earlier stale-session explanation. These are recorded hypotheses, not reproduced findings in this task.
- That investigation attributes missing mod logs in tester bundles to the manager excluding *_log.txt; no new server bundle inspection performed here.
- Public fetches were read-only via urllib with browser User-Agent; ZIP inspected in memory. No game files, production artifacts, SDK findings or product lifecycle state changed.

## Changes and restoration
New record: this file. Component README gets latest-release notice and index link; older entries retained as historical evidence. Complete README before-state preserved under workspace_organization_audit/MULTIPLAYER_CHECK_BEFORE_2026-10-14 (filename recorded below). Restore README by copying those bytes back. No binaries changed.

## Pending validation and follow-up
- Correct the two stale usage-test version expectations, then rerun the usage script before claiming all automated checks pass.
- Two-PC exact-1.0.1 confirmation: blocked manifest UDP must no longer remove the modded client.
- Obtain host/client multiplayer_overhaul_log.txt and host vehicle Classes lines for the wrong-season report; retest teleporting with host Modern selected. No cause proven here.
- Full multiplayer join/leave, session transition, AI and single-player regressions remain subject to recorded release gates.
- Documentation gate: this UTF-8 record is linked from the normal component README; facts, interpretation and pending validation separated. Post-documentation validator result appended below.

Wasted-effort check: initial combined cold-start read exceeded output limit and required a focused reread; no unnecessarily broad build; no command retries otherwise; no polling/idle wait.

README before-state: workspace_organization_audit\MULTIPLAYER_CHECK_BEFORE_2026-10-14\coop_mode_README.md

Post-documentation workspace validator: unchanged two errors (.claude, plans); zero warnings. No structural changes.
