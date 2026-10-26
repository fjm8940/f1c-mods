# Mechanical Failures x CO-OP: sync investigation + spec (2026-10-10, Claude)

Owner request (chat 2026-10-10): "make mechanical failures CO-OP compliant so failures happen in sync with everyone".
Pipeline stage: **investigate -> specify**. No code changed. Implementation waits for owner GO.

## Verified facts (static, source read 2026-10-10)
| # | Fact | Source |
|---|---|---|
| F1 | MF has **no** multiplayer/CO-OP awareness (no game-mode, network-state or host check). | grep of `dll/*.c`, `dll/*.h` |
| F2 | Every player family hardcodes **slot 0** as "the player": `VEH_BASE 0x5EC6D8 /* slot 0 = player */` (engine 83 uses, gear 22, electronics 9, suspension `PLAYER_VEH` 15, `mf_audio.h` 3). | `engine_failure.c:647`, `gear_failure.c:64`, `electronics_failure.c:85`, SDK `suspension_failure.c:119` |
| F3 | In CO-OP the local human is the slot with controller marker `veh+0x32C8 == 0`; host = slot 0, a client is **slot 1+** (Tester A = slot 1 live). On a client, slot 0 is the **host's car** as a remote copy (marker 2). | `coop_mode.cpp:506 local_human_slot()`, `IMPLEMENTATION_LOG_2026-10-10.md:119,123` |
| F4 | MF AI model covers slots 1..23 only (`MF_AI_MAX 24`) and skips any car whose marker != 1 (audit MF-12). | `mf_engine_model.h:441`, `engine_failure.c:8695` |
| F5 | CO-OP bots live in **slots 32-54**. Host: marker 1 (local AI). Client: marker 2 (remote copy). | `coop_mode.cpp:2052`, `IMPLEMENTATION_LOG_2026-10-10.md:123` |
| F6 | AI seed is per-PC (`GetTickCount` mix) and the engine model is driven by local rpm/throttle; two PCs can never compute the same AI failures independently. | `engine_failure.c:7885` |

## Consequences today (inference from F1-F6, not live-tested)
1. **Client corrupts the host's car copy.** A client's MF reads/writes slot 0 = host remote copy: player engine/gear/elec/susp faults, timer writes, retire would hit the wrong car on that PC only, while the client's own car (slot 1) gets nothing. Desync + possible wrong retirements. **Highest risk.**
2. **CO-OP bots never get mod failures** (slots 32+ outside MF_AI_MAX). Only the vanilla AI engine timer can kill them, on the host. So bots are "in sync" only because MF ignores them.
3. **No presentation sync.** A host player fault (smoke, fire, blow-up, limp) is not shown on clients beyond what native car replication carries (position/speed). Smoke/flames/messages are local effects.
4. Retire propagation of a bot/human across PCs is unproven (CO-OP framework S4 "retire/DNF" still OPEN). Open lead: host slot 0 found `retired=1 FinishState=2` in the 2026-10-10 race with no CO-OP log cause (`IMPLEMENTATION_LOG_2026-10-10.md:232`) — MF on host is one candidate; unverified.

## Design: host-authoritative fault events
Principle (matches `coop_mode/docs/DEFINITION_SYNC_FRAMEWORK_2026-10-09.md`): **the PC that simulates a car owns its failures**; others only replay presentation.
- Owner of a human car = that human's PC. Owner of a bot = host.
- Owner PC runs the full MF model for its cars; on onset / stage change / terminal it emits an event.
- Other PCs never model that car (already true via marker check once slot logic is fixed); they apply the event's **presentation only** (smoke/flames particles at the car, HUD/radio/commentary line "X engine failure", retire notice). Car motion stays native-replicated.
- No shared seed needed: no PC re-simulates another PC's car.

### Event message (proposal)
`MFEV v1 | session_gen | seq | owner_slot | car_roster_index | family(engine/gear/elec/susp) | fault code | stage | flags(retire, blow, fire) | session_time`
Reliable, idempotent by (owner, seq). Late joiner gets current state snapshot per car.

### Transport choice (decision for owner)
- **A (recommended):** CO-OP DLL exposes a tiny in-process API (named shared memory ring `Local\COOP_MF_EVENTS_V1` + exported `coop_peer_send`), carries events on its existing UDP/TCP 34500 link. MF stays transport-free and inert when CO-OP is absent.
- B: MF opens its own socket. Rejected: second port, NAT/firewall burden, duplicates CO-OP session logic.

## Phases
| Phase | Scope | Test |
|---|---|---|
| P1 safety | Resolve local player slot (marker 0) instead of slot 0 in all four families + audio; if no marker-0 car, player model idle. Single-player unchanged (slot 0 = marker 0). | Unit: slot resolver; SP regression (exact-hash) ; CO-OP client: faults land on own car, host car untouched |
| P2 bots | Extend AI model to host-owned bots in slots 32-54 (`MF_AI_MAX` 24 -> 64 or slot map; shared-memory ABI `ai_slot_count` bump; test panel update). Host only (marker 1). | Host log shows bot onsets in CO-OP |
| P3 events | CO-OP event channel (option A), MF emitter on owner PC, receiver presentation on peers. | Two-PC: forced fault on host bot + host player + client player; all screens show smoke/notice within 1 s |
| P4 retire | Retire/DNF propagation, depends on CO-OP S4 retire path. | Results identical on all PCs |

## Abort conditions
- Any SP behaviour/hash change beyond P1's resolver: stop.
- If remote copies already show native smoke/retire (replicated by NetComm), drop that part of P3 — check live first.

## Pending validation / open questions
- Live: does vanilla NetComm replicate a remote car's retire state and smoke? (decides P3/P4 size)
- Live: confirm client MF today writes slot 0 (enable MF on laptop, force engine fault, watch host car copy).
- Owner: GO for P1 (small, safety); transport A vs B; implementer (Claude vs DeepSeek batches).

## Implementation 2026-10-10 (Claude, owner: "do all these fixes, you do them; must NOT change single-player")
Stage: **implement -> verify**. Built and offline-tested only. **No in-game test, nothing installed, nothing shipped.**

### Single-player guarantee (design)
All CO-OP behaviour is behind the bridge block `Local\COOP_MF_BRIDGE_V1`, created only by the CO-OP DLL and
marked active only inside a CO-OP session (network state 1/2/3 with CO-OP selected). Without it:
player = vehicle slot 0, MF AI index = vehicle slot (identity), every new hook returns at once.
Evidence: `tools/coop_sp_identity_test.c` PASS (no block, inactive block -> slot 0, base 0x5EC6D8, no game memory read).
Inference, not proven: the release binary changes (new code), so an SP in-game regression check is still required.

### Files
| File | Change |
|---|---|
| `dll/coop_mf_bridge.h` (new, identical copy in `coop_mode/dll/`) | shared ABI v1: rings, event types |
| `dll/mf_coop.h` (new) | bridge open, `mfc_player_vslot()` = marker-0 slot in CO-OP, else 0 |
| `dll/mf_coop_engine.h` (new) | host AI index map (bots slots 32-54 -> MF index 1..23), event send/receive, remote effect queue |
| `dll/engine_failure.c` | player `VEH_BASE+off` -> `PLR_VEH` (33 sites); AI `VEH_BASE+stride*slot` -> `MF_AIV(slot)` (36); veh->slot via `mf_islot` (3); pull-off layout bound and traffic scan cover all slots in CO-OP; mirror hooks in `pq_push`, `blow_fx_claim/release`, `play_at_car`, `ai_play_fx_at`, `ai_exhaust_pop_at`; remote queue drained in the fuel-cut hook; INI `[CoOp] SyncFailures`, `SyncRetire` (default 1) |
| `dll/gear_failure.c`, `dll/electronics_failure.c`, `dll/mf_audio.h` | player base via `mfc_player_base()` |
| SDK `mods/sdk_sandbox/suspension_failure/suspension_failure.c` | `PLAYER_VEH` now `#ifndef` (standalone build unchanged); merged build force-includes `dll/mf_coop_susp.h` |
| `dll/build_merged.bat` | `-include mf_coop_susp.h` for the suspension TU |
| `coop_mode/dll/coop_mode.cpp` | creates the bridge, `MFEV1` datagrams on ManifestPort: client -> host, host -> all clients (+relay), reliable x3 for fire/retire, peer list from manifest requests, recv timeout 500 -> 50 ms; INI `[CoOp] MfSync=1` |
Before-states: `*.BEFORE_20261010_coop` (MF), `*.BEFORE_20261010_mfsync` (CO-OP).

### Events mirrored (owner PC -> every other PC, applied only to marker-2 copies, matched by slot + driver name)
Particle bursts (blue smoke, flames, black smoke), smoke banks veh+0x3D54/58/5C (player: physics-object banks),
blow-up fire descriptor tuning, positional sounds (MF wavs, game backfire samples), retirement (FinishState 2 ->
receiver writes ReasonCode + FinishState 2 on its copy if still 0).
Not mirrored: player oil trail / steam emitters that bypass the burst queue, HUD/radio/voice (owner-only by design),
gear/electronics/suspension effects (dev families; release is engine-only).

### Verification done
- Builds: MF dev merged `e617396d...` and release-style engine-only `0c684dc9...` (both `%TEMP%`, -Werror clean); CO-OP `714fdee5...` (`%TEMP%\coop_mf_build`).
- Tests PASS: `coop_mode/tests/test_coop_bridge_abi.py` (new: header copies identical, layout, rings), CO-OP address inventory 57, menu logic, target probes, usage; MF `mf13_inventory_test.py` 85/85, `audit_proof_test.py` 47/0, `runtime_hook_test.py`, `tool_safety_test.py`, `usage_test.py`, `coop_sp_identity_test.c`.

### Pending validation (owner, two PCs)
1. SP regression: normal race with the new MF DLL, failures behave as 1.0.4 (log has no `COOP:` lines).
2. CO-OP host log: `COOP: CO-OP session (host)`, `AI index N = vehicle slot 32..`; bot faults appear (they never did before).
3. Client: own-car fault hits slot 1 car (not the host car copy); host sees smoke/fire/sound on that car.
4. Host bot blow-up: client sees fire + smoke + sound; retire shows on the client's timing (SyncRetire).
5. Unknown to check: does native rendering use banks written on a remote copy; does NetComm overwrite FinishState on copies.
Abort: any SP difference -> set nothing, revert to `*.BEFORE_20261010_coop`.
