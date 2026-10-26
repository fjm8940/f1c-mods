# Red Flag — source-vs-shipped check for 1.1.6 (2026-10-08)

Status: **STOP — source is stale. No 1.1.6 edits, no PIN update made.**

This record completes the first, gating step of the recorded Red Flag 1.1.6
resume (`development/mods/DS_RESUME_AUDIT_CLOSEOUT_2026-10-08.md`, "RF." and the
"AUDIT RF source-vs-shipped check (2026-10-08)" entry in
`mechanical_failures/docs/ENGINE_SIM_MODEL_DEEPSEEK_HANDOFF_2026-09-30.md`).

That step is: "confirm first that `src/gr44.c` equals the shipped 1.1.5 base
(regenerate through make_release.py and compare with the shipped hardened
source/DLL). If they differ: STOP and report."

## Result (fact)

`src/gr44.c` does **not** reproduce the shipped Red Flag 1.1.5. The canonical
dev source is stale relative to the shipped release.

| Artifact | SHA-256 |
|---|---|
| `src/gr44.c` | `7411BE1696D42F5584229D53299615F348BBBD30B9B1842D09E7518518C8E817` |
| `generated/redflag.c` (on disk) | `871DA8A7732304EFDC1E9E90B35E1247036FA5E4140F408D89CA4993CABB4698` |
| `make_release.py(src/gr44.c)` in a `%TEMP%` sandbox | `871DA8A7732304EFDC1E9E90B35E1247036FA5E4140F408D89CA4993CABB4698` (equals on disk) |
| `harden_release.py(generated/redflag.c)` | `A72C8574786A69311025EF49811ED107F118B621501C506BD1141B794C354284` |
| shipped `releases/v1.1.5-rc.2/redflag.hardened.c` | `A0C471BBAFF34B844C341ABD8485E73F44D9D10AD9282DDACEE4B1FE939F6B26` |
| rebuilt `releases/v1.1.5-rc.2/build.ps1` DLL | `43050C2DFD607EC4EC2C0702C2096261EA903B1EE78D0C18EB504DD6D0940EEC` (matches the live catalog 1.1.5 DLL) |
| `releases/v1.1.4-rc.4/redflag.hardened.c` | `0A0F7E1916F64BFA9251F951ED5CC2EBC3896B46F62D9572798B8AEB0545D77F` |

- `src/gr44.c -> generated/redflag.c -> harden_release.py PIN` are internally
  consistent (the PIN equals the on-disk generated source). This is what the
  earlier handoff entry verified.
- The standard pipeline output (`A72C8574`) differs from the shipped hardened
  source (`A0C471BB`) in **8 hunks / 4 changed functions or gates** (list
  below). The shipped hardened source *does* rebuild byte-for-byte to the
  shipped catalog DLL `43050C2D`.
- The `A72C8574` hardened source also **fails to build**: `harden_release.py`
  writes only after its source PIN matches; the mismatch blocks regeneration
  of a release from `src/gr44.c` today.

## Diff review: shipped 1.1.5 base vs the `src/gr44.c` pipeline

Distinct changes in `releases/v1.1.5-rc.2/redflag.hardened.c` that are absent
from the pipeline output:

1. `#include <tlhelp32.h>` added (supports the new module scan).
2. `rf_loader_exclusivity_matches()` (host exclusivity gate):
   `RFP_MIN_GENERATION 1u` -> `RFP_HUB_2_GENERATION 2u`; new
   `RFP_CAP_HUB_2 = 0x...2`; the gate now requires
   `(RFP_CAP_EXCLUSIVE_HOST | RFP_CAP_HUB_2)` and
   `compatibility_generation >= RFP_HUB_2_GENERATION`.
3. New static `rf_find_broker_module()` (Toolhelp module scan for the one loaded
   module exporting `F1CBrokerGetInfo`); `rf_broker_bind()` and
   `rf_broker_bind_after_dependency_wait()` now call it instead of
   `GetModuleHandleA("f1c_game_update_broker.dll")`.
4. `RF_HOST_GATE_TEST` early-return block inside the protected loader-start
   entry (`rf_secure_loader_start` / `rfe_1d3b7a91`).

Change 2 is already present in `releases/v1.1.4-rc.4/redflag.hardened.c` and
change 4 is there too; both are absent from `src/gr44.c`. So `src/gr44.c`
predates 1.1.4-rc.4 and is stale by at least the 1.1.4 host-gate work plus the
1.1.5 export-scan work. The 1.1.4/1.1.5 releases were produced by patching the
hardened release source directly, bypassing the `src/gr44.c` pipeline.

## Why this blocks 1.1.6 (inference)

Implementing 1.1.6 on `src/gr44.c` as it stands and shipping it would:
- revert broker discovery to `GetModuleHandleA("f1c_game_update_broker.dll")`,
  which stays inert under Lightloader 2's built-in broker (the exact defect
  1.1.5 fixed); and
- drop the HUB_2 host gate and host-gate-test path.

That is a functional regression, not a version bump. Per the recorded plan and
the owner's rule ("PIN update only after the diff review"), no change was made.

## Correction to the earlier handoff entry (fact)

The entry "AUDIT RF source-vs-shipped check (2026-10-08)" concluded "src/gr44.c
... IS the canonical shipped 1.1.5 base and the RF work may proceed". It
compared only `make_release` output against the PIN, not the shipped hardened
source/DLL. That conclusion is incomplete and is superseded by this record.

## Recommended next action (needs owner/Claude decision)

Re-sync the canonical dev source before any 1.1.6 work:
1. Bring the three 1.1.5 changes (HUB_2 gate, export scan, host-gate test) into
   `src/gr44.c` (or adopt the shipped hardened source as the new base) and
   confirm the pipeline reproduces `43050C2D` exactly.
2. Only then implement 1.1.6 (rf_usage.h on f1c_usage.h; log tap; shared VEH
   install-after-init / remove-on-unload; session-end summary via the broker +
   host-unload flush; version 1.1.6; `-I mods/sdk`; meta minimum covering used
   broker capabilities; regenerate + harden with the PIN updated only after the
   changed-function diff review).

## Pending validation

- Owner/Claude confirmation of the re-sync approach (adopt shipped base vs
  reconstruct in `src/gr44.c`).

## Wasted-effort check

- Repeated reading: none material; the 11 k-line `gr44.c` was read only in the
  targeted regions needed.
- Over-broad builds: one baseline build of the shipped release + one pipeline
  regeneration in `%TEMP%`; both necessary to compare exact hashes.
- Failed commands: one PowerShell heredoc attempt failed (PowerShell 5.1 has no
  heredoc); replaced with a temp Python script immediately.
- Waiting/polling: none.
