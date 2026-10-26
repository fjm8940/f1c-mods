# Definition: every shipped mod synced in CO-OP (2026-10-10)

Stage: define. Owner request: "make sure all the mods are synced to work in multiplayer/co-op".
Baseline: COMPAT_AUDIT_SHIPPED_MODS_2026-10-10.md.

## Pattern to reuse (FACT)
Mechanical Failures already syncs through coop_mode: named shared-memory bridge (coop_mf_bridge.h ABI v1),
events carried over the CO-OP UDP link (ManifestPort 34500), host authoritative, cars identified by driver
name. Generalise to a per-mod event channel (same bridge layout, own tag per mod) instead of new sockets.

## Work items (one spec + build + live test each)
| # | Mod | Today in CO-OP | Needed | Size |
|---|---|---|---|---|
| 1 | Mechanical Failures | synced, MfSync=1 on owner PC | two-PC live test; ship MfSync=1 default | small |
| 2 | Pitstop Failures | host only, unsynced | host decides stop failure + extra time; clients play the same SFX/delay for that car | medium |
| 3 | Safety Car | refused in MP | host runs SC logic over all slots incl. 32+; send SC state, deploy/in, pace and order rules to clients; client human must obey pace/no-overtake | large |
| 4 | Red Flag | refused in MP (fail-closed) | host raises/ends red flag; clients show flag, stop cars, restart order from host | large |
| 5 | Driver AI Toolkit | released in MP | AI runs only on host; allow levers on host when CO-OP host (no client part) | small-medium |
| 6 | ESC Pause Menu | vanilla ESC in MP | network game cannot pause; menu without pause, or leave as is | owner decision |
| 7 | Extended Damage Commentary, Camera Editor, CLK55 | work locally | nothing (local per player) | none |

## Abort / guard rules
Each mod keeps its single-player path unchanged when no bridge is active. Plain MULTIPLAYER (not CO-OP)
stays vanilla. Production copies change only through the normal release pipeline.

## Open owner decisions
Order of work; ESC menu behaviour in MP; whether clients may trigger SC/red flag or only the host.
