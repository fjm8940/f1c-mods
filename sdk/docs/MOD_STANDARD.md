# F1C mod standard (every new and existing mod)

Status: mandatory development standard, owner-requested 2026-10-08 ("everything pipelined and
standardized"). Applies to every first-party Lightloader mod: new mods from day one, existing mods
at their next release. A mod that cannot meet a rule records the exception, with the reason, in its
README before release.
Related: `governance/PIPELINE.md` (lifecycle), `f1c_ida_decompiled/tools/sdk/NEW_MOD_GUIDE.md`
(SDK mechanics), `SPEC_AUDIT_CLOSEOUT_DS_2026-10-07.md`
(where these rules came from).

## 1. Lifecycle (no shortcuts)
`define -> investigate -> specify -> implement -> verify -> owner test -> release prep -> deploy -> observe`
- **Define:** a README with the objective, non-goals, acceptance criteria and the product id
  (snake_case, permanent).
- **Investigate:** SDK front door first (`python tools/f1c_mod.py start --json`,
  `evidence-query findings`, `inspect`). New facts go into `f1c_ida_decompiled/findings.jsonl`.
- **Specify:** Claude writes the spec with batches and STOPs.
- **Implement:** DeepSeek implements mechanical batches; Claude reviews each STOP (it re-checks hashes
  and reruns tests).
- **Owner test:** the exact release hash in game. A dev build passing is not a release pass.
- **Deploy:** Claude or the owner runs the scripted deploy (section 7) after the owner's "GO".

## 2. Folder layout
```
development/mods/<id>/
  README.md            objective, status, index of records (newest first)
  dll/ or source/      canonical dev source (dev and release source must match; re-sync on release)
  tests/ or tools/     all tests, runnable headless
  releases/v<X.Y.Z>/   frozen release source + build.bat + meta + install.txt
  docs/                specs, handoffs, findings
```
Created with `python tools/f1c_mod.py init <id>` (SDK template), then moved into this layout.
Test output and scratch builds go to `%TEMP%`. Before editing a file, keep a `.BEFORE_<date>_<tag>` copy.
Never delete; superseded builds move to `build_prev_<hash8>/`.

## 3. Required runtime pieces (checklist)
| # | Rule | Reference implementation |
|---|---|---|
| R1 | Loads through the Lightloader protected host entry (`rfe_*` export); no work in `DllMain` | Safety Car 0.10.91 |
| R2 | Registers with the shared broker; all game calls and sim writes run on the **game thread** (broker callbacks). Worker threads only read stable state, do file I/O and post to the broker. | MF 1.0.2 (MF-09), Pit 1.0.8 |
| R3 | No cross-thread `SessionType` or vehicle-state writes. Player retire = broker native retire route. | `project_native_retire_route`, Pit 1.0.8 |
| R4 | Startup that must catch a one-shot game event (vehicle preload) registers **within about 1 s of start**. No slow work (crypto, network) before the registration. Log the startup ms. | SC 0.10.91 (0.10.90 failure record) |
| R5 | Every patch is reversible and all-or-nothing: check every site first, write transactionally, roll back on failure. Never overwrite a foreign hook. | EDC 1.0.2, `f1c_hook.h` |
| R6 | Compatibility gate: the exe fingerprint, plus a maintained **address inventory** of every fixed address the mod touches, with a test that the source matches the inventory. | MF 1.0.2 `MF13_ADDRESS_INVENTORY.md` |
| R7 | Multiplayer and replay gated explicitly (refuse or read-only). | MF-12 |
| R8 | Shared AI levers: precedence **race control (SC/RF) > Mechanical Failures > AI Toolkit** (owner, 2026-10-08). Write only levers you own; restore only when the live value is still your own write; respect the session generation. | MF COMP-01b, Batch C |
| R9 | Session boundaries: clear state on session end and generation change; survive 3 race loads in one process. | SC-01 |
| R10 | Settings in `<id>.ini` beside the DLL; a missing or invalid key falls back to a safe default; volume 0 = silent. | MF-10 |
| R11 | Developer-only tools behind a compile flag (`<ID>_DEV_TOOLS`), proven absent from the release object. | MF-11 |

## 4. Logging and usage reports (consent-gated)
- Local log `<id>_log.txt`: bounded, written by the worker, never from a game callback. Hardened builds
  log event codes (`SC-E…` pattern) and keep a private decode map beside the build.
- Usage summary: include `f1c_ida_decompiled/mods/sdk/f1c_usage.h` and add a small `<id>_usage.h`
  (reference: `pit_usage.h`, `esc_usage.h`, `sc_usage.h`). It writes
  `plugins\lightloader\reports\usage\<id>-<utc>-<salt>.json` **on session end, unload and crash**
  (a crash summary from a vectored exception handler, once per process).
- **Crash capture (owner 2026-10-11, mandatory):** the shared `f1c_usage.h` reports fatal-class
  exceptions in ANY module (not only the exe or the mod's own image; kernelbase/kernel32 probe faults
  excluded), and once per process writes a `crash_detail` summary (fault module id + offset, op/target,
  registers, module:offset stack, loaded-module ids) plus a local minidump in `reports\crash\` (max 5,
  never uploaded by the DLL). A chained unhandled-exception filter repeats it with `fatal 1`. Call
  `f1c_usage_crash_identity("<id>", "<version>")` at start; `f1c_usage_veh_install` derives the id
  from the DLL name when it is missing (version then `unknown`). Module ids: table in `f1c_usage.h`.
  Test: `f1c_ida_decompiled/mods/sdk/f1c_crash_capture_test.c` + `f1c_crash_capture_check.py`.
- Content: event codes, counts, timings, versions and hashes only. No names, paths, user text or raw
  log lines. `test_usage.py` must pass the words-only filter with injected names and paths.
- **The DLL never sends anything.** The Lightloader manager uploads only when the player has turned on
  "Send crash, bug and mod usage reports automatically". A new mod id needs: the manager `UsageMods`
  entry, the telemetry `USAGE_MODS` entry, and a privacy-page paragraph, all in the same release.

## 5. Metadata and versions
- `<id>.meta.json` embedded as `F1C_MOD_META` (id, name, version, author `Matias`, description in
  player wording, `paid`, `settings`, `lightloader: ">=X.Y.Z"` = the oldest loader with every broker
  feature used).
- Semver. **Any shipped byte change = a new version.** Never reuse a version label for different bytes.
## 6. Verification gates (all must PASS before owner test)
1. `build.bat` with `-Wall -Wextra -Werror`, run in a clean tree.
2. Unit and replay tests for every rule above that the mod uses (usage, patch rollback, lever
   precedence, inventory, session reset).
3. Final-artifact test of the **exact release DLL**: it loads, refuses foreign hosts and has no dev strings.
4. Startup timing logged (R4).
5. Owner in-game test of the exact hash: fresh launch, feature used, 3 race loads, logs checked by Claude.
If a build is not byte-reproducible, release pins the exact tested binary; never rebuild at release.

## 7. Release and deploy pipeline
- Release folder `development/lightloader/v2.0.0-hub-dev.1/release_<what>_<date>/`, cloned from the
  last release (current reference: `release_sc_0.10.91_2026-10-07`):
  `make_catalog.py` (from the **live** catalog, read-only fetch, changing only the released entries),
  `validate_all.ps1` (every supported manager, including the live one), `stage/`, `SHA256SUMS.txt`,
  `deploy_*.sh`, `rollback_*.sh`, `README.md`.
- Deploy script rules: pin the **current** live hashes, back up everything first, use `sudo -n test`
  for protected paths, hash-check each step, restore automatically on failure, check from outside
  at the end. `bash -n` clean. Nothing deleted.
- Changelog entry in `operations/release_management/changelogs/<id>.json`, plain player wording.
- Production copy `production/mods/<id>/v<X.Y.Z>/` (plus `SHA256SUMS.txt`) and `production/RELEASE_REGISTRY.md`.
  Superseded releases are marked withdrawn, never deleted.

## 8. Records (documentation gate)
Every batch leaves its record in the mod's docs. The README index points to it. Facts are separate
from inferences, and pending owner checks are named. Release state goes into
`governance/PRODUCT_STATUS.json` only when the evidence changes.

## 9. Existing mods: compliance tracker
Audit close-out batches (`SPEC_AUDIT_CLOSEOUT_DS_2026-10-07.md`) bring the shipped mods to this
standard. Status as of 2026-10-08:
| Mod | Missing |
|---|---|
| Safety Car 0.10.91 | compliant (R1-R11, usage, DRM) |
| Mechanical Failures 1.0.2 | ships in the combined release (Batch R) |
| Pit Stop Failures 1.0.8 | ships in R (usage + crash summary done; manager/telemetry/privacy onboarded in U3) |
| ESC Pause Menu | 1.0.8 + crash summary + protected pipeline (usage onboarded in U3) |
| Red Flag 1.1.6, EDC 1.0.3 | usage reports done (U2); onboarded in manager/telemetry/privacy in U3 |
| AI Toolkit, Camera Editor | usage reports + Toolkit precedence; WAIT per owner (2026-10-08) — no U2 group 3 |
Update this table at each release.
