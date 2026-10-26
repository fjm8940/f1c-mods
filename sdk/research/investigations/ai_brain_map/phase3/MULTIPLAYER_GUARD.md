# MULTIPLAYER_GUARD - offline-only enforcement for the public AI Brain Lab (batch E)

Prepared 2026-10-03, batch E of `tools/sdk/DEEPSEEK_HANDOFF_AI_BRAIN_PHASE3_2026-10-02.md` (the
"BATCH E - multiplayer detection" section). Static research, offline only: no game access, no
attach, no injection, no writes. All statements come from the decompiled corpus + active docs in
this repository. This file is the only artifact created by the task.

Release context: `development/mods/ai_brain_lab/SHARE_PLAN_2026-10-03.md` section 5/6 - the plugin
must refuse to act in any multiplayer / network session; that refusal is a RELEASE BLOCKER for the
public, offline single-player-only release. The plugin currently has no session check (grep of
`ai_brain_lab.c` / `abl_logic.h`: only `O_LIVE 0x32C8`, no session/MP logic).

Verdict in one line: **guard on `*(int*)(*(uintptr_t*)0x7CD25C + 0x3FC4) == 0` (no network
session) AND `*(int*)(veh + 0x32C8) == 1` (this car is locally AI-simulated), fail-closed on any
unreadable value.** Both are the game's own checks; see sections 3 and 4.

---

## 1. Prior art consumed

- Finding `f1c-multiplayer-detector-gamemodulemanager-3fc4` (DERIVED, 2026-08-02): state =
  `*(*(u32*)0x7CD25C + 16324)` (GameModuleManager +0x3FC4); enum, not bool; **offline value
  measured 0 live** (pid 11568, `*(0x7CD25C)=0x04630048`, `*(0x0464636C)=0`); the nonzero-in-MP
  half was UNVERIFIED at the time. `MultiplayerVehiclePlacement` takes its MP branch under
  `SessionStateCheckWrapper != 0`; sibling `StateCheckWrapper` tests `== 1 || == 2`.
- `investigations/MULTIPLAYER_NETWORK_STATIC_MAP_2026-08-30.md`:
  - line 33: `GameMode 0x46D6D0` routes mode 5 into multiplayer **after `SessionStateCheck == 3`**.
  - line 79: `CollisionEventNetworkBroadcast` sends only when session field `+0x3FC4` is 1 or 2.
  - line 40: `MultiplayerSessionJoinHandler` sets multiplayer flag `dword_7C56A0 = 2`.
  - line 46 (inference): states 1/2 = host/authority paths, 3 = client.
- `investigations/ai_interaction_role/README.md` lines 4433-4453: `NetCommPlayerSlotSync 0x45FC50`
  calls `SessionStateTransition(V+0xC, 0, 0)` for the local player slot and
  `SessionStateTransition(V+0xC, 2, 0)` for a non-player slot - "Network slot sync selects packet
  mode for non-player vehicles". `SessionStateTransition` callers request -1/0/1/2/3 across
  lifecycle systems.
- `mods/red_flag/redflag.c` lines 1992-2005: the only existing in-repo user of the detector.
  `mp_state()` reads `*(0x7CD25C) + 16324` with `IsBadReadPtr`; `is_multiplayer() = state > 0`;
  deliberately **fail-open** (-1 unreadable and 0 offline both allow) because red_flag is private
  and "blocking on a detector that has never been seen working would break the offline mod".
  The comment also records the live offline value **0** ("*(0x04630048 + 16324) = 0").
  For the public plugin the required posture is the opposite: **fail-closed**.
- Findings queried for `multiplayer`, `network`, `online`, `host`, `client`, `session`, `packet`,
  `VehiclePhysicsTimeStep`, `32C8`: nothing contradicts the above; the packet/replay boundary
  material is in the ai_interaction_role README and the multiplayer map.

Evidence tiers used below: OBSERVED = live read documented in a finding/source; DERIVED = static
code read; GUESS = not proven. No FACT (FACT needs a live poke; none performed).

---

## 2. Candidate signals

| id | signal | address / expression | type | offline | host | client | tier | verdict |
|---|---|---|---|---|---|---|---|---|
| S1 | session state (GameModuleManager +0x3FC4) | `*(int*)(*(u32*)0x7CD25C + 16324)` | int enum | 0 | 1 or 2 | 3 (4 transient) | OBSERVED offline=0; DERIVED semantics | **primary guard** |
| S2 | MultiplayerFlags | `dword_7C56A0` | int flags | 3 | 3 | 2 | DERIVED | secondary / cross-check; cannot separate offline from host |
| S3 | per-vehicle physics mode | `veh + 0x32C8` | int enum | player 0, AI 1, empty -1 | player 0, local AI 1, remote human 2 | own 0, others 2 | DERIVED | **per-car guard** |
| S4 | race/session type selector | `dword_7D0410 == 5` | int mode | not 5 | 5 | 5 | DERIVED for MP selection; OPEN for in-race stability | log first, optional third check |
| S5 | session/network delegate pointer | `*(u32*)(*(u32*)0x7CD25C + 0x3FB8)` | ptr | nonzero offline | nonzero | nonzero | DERIVED | auxiliary, not a discriminator |
| S6 | SessionInitState | `dword_5CC9E8` | int flow state | -1/0/1/3/5 | same flow values | same flow values | DERIVED | **rejected** (session-UI flow, not MP) |
| S7 | SessionType | `dword_7C56A4` | int | 1 practice / 5 quali / 7 race | same | same | CONFIRMED (`docs/38`) | **rejected** (not an MP signal) |

---

## 3. Signal detail

### S1 - GameModuleManager +0x3FC4 (RECOMMENDED PRIMARY)

Address: global pointer `dword_7CD25C` (0x7CD25C, the GameModuleManager singleton, also documented
in `docs/12-globals-reference.md` line 2491); field at +16324 (0x3FC4). This is the game's own
multiplayer/session-state enum.

Writers (static, all confirmed by reading the function bodies):

| writer | address | writes | file:line |
|---|---|---|---|
| `AudioChannelInit` (manager constructor path) | 0x45D530 | 0 | `functions/AudioChannelInit_0045d530.c:84` |
| `ShortComputation_3140` | 0x463140 | 0 | `functions/ShortComputation_3140_00463140.c:6` |
| `sub_463EF0` (session setup reset) | 0x463EF0 | 0 | `functions/sub_463EF0_00463ef0.c:7,37` |
| `DriverEntrantSetupEntry` (network entrant setup) | 0x462550 | **1 or 2** | `functions/DriverEntrantSetupEntry_00462550.c:63-66` |
| `CollisionSpatialQuery` | 0x462CE0 | 4 (transient) | `functions/CollisionSpatialQuery_00462ce0.c:10` |
| `SessionStateCheck` (normalizer) | 0x461CD0 | 4 -> 3 | `functions/SessionStateCheck_00461cd0.c:3-7` |

`DriverEntrantSetupEntry` writes `1` when `BitFlagTestAt0x6E(a2,0)` set else `2`; `a2` is the
128-byte entrant/join record. The exact 1-vs-2 meaning (e.g. dedicated vs normal host) is not
resolved statically; **both are refused**, so it does not affect the guard.

Readers proving the semantics (the game itself):
- host/authority: `CollisionEventNetworkBroadcast 0x462940` sends only `if (v3 == 2 || v3 == 1)`
  (`functions/CollisionEventNetworkBroadcast_00462940.c:10-12`);
  `StateCheckWrapper 0x461DC0` returns `state == 2 || state == 1`
  (`functions/StateCheckWrapper_00461dc0.c:5-6`).
- client: `GameMode 0x46D6D0` case 5 (multiplayer) runs only when
  `SessionStateCheck(dword_7CD25C) == 3` (`functions/GameMode_0046d6d0.c:22-36`);
  `SynchronizeWithServer 0x464290` checks `== 3` (`functions/SynchronizeWithServer_00464290.c:17`);
  `ClearPlayerSlots` / `NetCommSessionMessageDispatcher` / `RunCollisionDetectionLoop` /
  `InputCommandDispatchToEntities` all treat `== 3` as the client state
  (rg `4081` results, listed in section 7).
- offline: value 0 is the reset state in every non-network setup path, and the live read on
  2026-08-02 was 0.
- transient: `CollisionSpatialQuery` writes 4; `SessionStateCheck` converts 4 to 3 when the
  session delegate reports 3 (`functions/SessionStateCheck_00461cd0.c:5-6`). 4 is never offline.

Reliability: high, because this is the field the game itself uses to select network vs local
behavior. It is not a pointer (safe to read after the manager exists) and has a small closed value
set. It cannot be confused by session type (S7) or UI flow (S6).

### S2 - `dword_7C56A0` "MultiplayerFlags" (secondary; do NOT use alone)

Writers (rg `7C56A0`; complete list in the corpus):
- `GameMainEntry 0x45B200:6` = 3 (boot), `Game_Post 0x42DA30:25` = 3 (session teardown),
  `Thunk_0042CE00:7` = 3.
- `SessionTypeSetter 0x463F70:29-38`: if GameModuleManager+0x3FC4 is 1 or 2 (host) -> **3**;
  otherwise -> **2**. It returns early (no write) when the state is 0 (offline): the value stays 3.
- `AutoJoinRetryLoop 0x4637A0:98` = 2 and `MultiplayerSessionJoinHandler 0x46DE10:100` = 2
  (client join paths).

Readers use **bit 0** as "this machine simulates locally": `sub_4FCEE0:53` and `sub_4FCC30:104`
run the local/authority branch on `(dword_7C56A0 & 1) != 0`; `CarPositionTargetCheck 0x4FE060:5`
runs the position-target branch on `(dword_7C56A0 & 1) == 0`.

Consequence: **3 = local authority (menu, offline race, AND host); 2 = network client.**
Therefore `dword_7C56A0` alone can never separate an offline session from a hosted session, and
the existing one-line meaning in `docs/12-globals-reference.md:108` ("bit 0 = multiplayer active")
is contradicted by the writer/reader set. Proposed correction (Claude owns the doc): "bit 0 =
local simulation authority (1 = offline or host, 0 = network client)". Use only as a cross-check
for the client direction (`== 2` -> refuse).

### S3 - per-vehicle `veh + 0x32C8` (RECOMMENDED PER-CAR GUARD)

One dword per vehicle slot (base `unk_5EC6D8`, stride 18624; interior physics object at `V+0xC`,
so `SessionStateTransition` writes it as `P[3247]` = `P+0x32BC` = `V+0x32C8`;
`functions/SessionStateTransition_004ef5a0.c:13-14`).

Values and writers:
- 0 = local player car. `NetCommPlayerSlotSync 0x45FC50:82-86`:
  `SessionStateTransition((float*)v6+3, 0, 0)` for the local node slot.
- 1 = locally simulated AI car. `AIDriverCountSetup 0x4BFD90:66,86,88`;
  `sub_4DA840:47` (player restart path), `RaceFinishHandler:9`, `RestartGate:22` for 0/1.
- 2 = network-synchronized non-player car (the packet route):
  `NetCommPlayerSlotSync 0x45FC50:90` `SessionStateTransition((float*)v6+3, 2, 0)` for every
  non-local slot; README 4433-4443.
- -1 = empty/unloaded: `NetCommPlayerSlotSync:252` (and slot tests `== -1` in
  `BuildActiveVehiclePointerArray`, `NetCommPlayerSlotSync:263`, `sub_46BA20:50,82`).
- 3 = loading/lifecycle states: `ProcessDriverEntries:129`, `LoadVehicleEntries:20`,
  `LoadingPhase1:33`, `LoadingPhase2:50`.

What the game itself checks before running AI on a car (direct answer to batch E item 2):
- `AIMainUpdate 0x424E70` first line: `if (*(_DWORD*)(this + 13000) > 1u) return;`
  (`functions/AIMainUpdate_00424e70.c:44`) - 13000 = 0x32C8. 0/1 pass, 2/3/-1 rejected.
- `MainGameTickUpdate 0x42C690:47-48` calls `AIMainUpdate(dword_7C54F0[i])` for every active
  vehicle every 10th tick, so the per-car gate is the game's own filter.
- Other AI subsystems additionally run only for `+0x32C8 == 1` (e.g. `WheelTireForceUpdate`,
  `FuelConsumptionAndWear` at `MainGameTickUpdate:71,82`; `AIPushLap:15`; `sub_426430:224`).

Reliability: high and local to the car. In a host, remote human cars are 2 (created by the same
slot-sync function), so a write path gated on `== 1` cannot touch another player's car even if the
global guard failed.

### S4 - `dword_7D0410 == 5` (log-only, optional third check)

Front-end race/session type selector, used as "multiplayer" by the game's own AI code and UI:
- writers: `sub_463BC0 0x463BC0:8` (host creation sets 5), `AutoJoinRetryLoop 0x4637A0:27`
  (client auto-join sets 5), `MainMenuTransition 0x404F30:15` (menu value), `RaceModeSetup
  0x404890:9,13,17` (temporarily 2, clamped into [7D0418..7D041C]).
- readers: `GameMode 0x46D6D0:9` case 5 = multiplayer screens; AI code branches on it:
  `AIOvertakeDefendStateChange 0x419690:25,38,45` and `AIFormationLap 0x41A3A0:19` treat
  `== 5` as multiplayer behavior differences.

Tier: DERIVED that MP selection = 5 for both host and client. Whether it stays 5 during an actual
in-race host session (vs being clamped/restored) is **OPEN**. Recommendation: log it, do not gate
on it until the live host test shows the in-race value.

### S5/S6/S7 - auxiliary and rejected

- S5 `*(u32*)(*(u32*)0x7CD25C + 0x3FB8)` (16312): the session/network delegate object pointer.
  Nonzero already offline (`AudioChannelInit` context, `SingleFunctionCallWrapper_bf50`); its
  vtable is called by `SessionStateCheck` only when the state is 4. Not a discriminator; useful
  only to explain state 4 -> 3.
- S6 `dword_5CC9E8` SessionInitState: session-UI flow (-1/0/1/3/5), same values offline and in MP;
  rejected.
- S7 `dword_7C56A4` SessionType: 1 practice / 5 qualifying / 7 race in single player
  (`docs/38-confirmed-offsets.md`); qualifying already equals 5, so it must never be used as an MP
  signal; rejected.

---

## 4. Recommended guard rule for the DLL (single rule, two conjuncts)

Act only when **both** hold; anything unreadable or anything else means refuse:

```c
#define GAME_MODULE_MGR   0x007CD25Cu
#define OFF_SESSION_STATE 0x3FC4   /* int: 0 no network session; 1/2 host; 3 client; 4 transient */
#define VEH_PHYSICS_MODE  0x32C8   /* int: -1 empty, 0 local player, 1 local AI, 2 network non-player */

/* -1 = cannot read; caller treats -1 as "not offline" (fail-closed). */
static int abl_session_state(void) {
    uintptr_t mgr = *(volatile uintptr_t*)GAME_MODULE_MGR;
    if (!mgr || IsBadReadPtr((void*)mgr, OFF_SESSION_STATE + 4)) return -1;
    return *(volatile int*)(mgr + OFF_SESSION_STATE);
}

/* True only for a locally simulated AI car in a session with no network state. */
static int abl_may_act(uintptr_t veh) {
    if (abl_session_state() != 0) return 0;                 /* 1/2/3/4/unknown -> refuse */
    if (!veh || IsBadReadPtr((void*)veh, VEH_PHYSICS_MODE + 4)) return 0;
    return *(volatile int*)(veh + VEH_PHYSICS_MODE) == 1;   /* remote packet cars are 2 */
}
```

Application points:
- Evaluate at **every** control write (lever set, rule patch, ARM action) and once per second
  while the plugin is loaded, because one game process moves menu -> offline race -> menu -> MP
  join and back; the DLL cannot assume the session state at load time.
- On refusal: release all active levers to their original values, disarm, disable the control
  drawer, and log one line with both raw values. Viewer-only reads may stay on (they do not
  "act"), but must not write.
- Keep the existing per-car `0x32C8` read for the viewer (`O_LIVE` in `ai_brain_lab.c:37`); the
  guard reuses it.
- Do not copy red_flag's fail-open choice (`redflag.c:2004`); for a public release, unreadable
  must mean refused.

Failure modes:
- **Unreadable S1 (mgr null, bad ptr, game closing) -> refuse.** Offline at boot/menu may be
  refused until the manager exists; safe direction, acceptable.
- **S1 wrong in the dangerous direction** (reads 0 while actually MP): the per-car conjunct still
  blocks writes to network-synced cars (0x32C8 == 2), but could not stop writes to host-simulated
  AI cars (0x32C8 == 1) in a hosted session. This is the one residual risk; the live host test in
  section 5 is what disproves it. As extra belt-and-braces, also refuse when `dword_7C56A0 == 2`
  (client) or - once the live host read confirms the in-race value - when `dword_7D0410 == 5`.
- **S1 wrong in the safe direction** (nonzero offline): plugin simply refuses offline; visible
  bug, no fairness risk. The single documented live offline sample is 0, and all offline setup
  paths write 0, so this is unlikely.
- **S3 wrong** (a remote car reads 1): not observed; `NetCommPlayerSlotSync:90` is the only
  writer for non-local slots and writes 2. Log the distribution as a tripwire.

---

## 5. Owner live test plan (requires the game; NOT run in this batch)

The plugin build used for this test must log S1 (`3FC4`), S2 (`7C56A0`), S4 (`7D0410`), every
car's S3 (`32C8`), and every guard decision (allow/refuse + raw values), to a file. Run the
known-good full-weekend path (no QuickRace).

1. **Offline race (should pass).** Start an offline race, arm one harmless lever on an AI car.
   Expected: `S1 == 0` all session, `S2 == 3`, `S4 != 5`, AI cars `S3 == 1`, player `S3 == 0`;
   control allowed and visibly applied. This is also the regression test for false refusals.
2. **Boot/menu fail-closed.** Load the plugin, sit at the main menu, try ARM. Expected: refused
   (no session manager state yet and/or no AI car with `S3 == 1`), one log line, no crash.
3. **LAN host (must refuse).** Host a session on machine A. Expected: `S1` becomes 1 or 2,
   `S2` stays 3, `S4` becomes 5; the plugin refuses every control write on every car, including
   host-simulated AI cars (`S3 == 1`). Record the host player car (`S3 == 0`) and any connected
   peer car (`S3 == 2`). **This is the test that proves the release blocker is closed.**
4. **LAN client (must refuse).** Join from machine B. Expected: `S1 == 3`, `S2 == 2`, `S4 == 5`,
   own car `S3 == 0`, all others `S3 == 2`; refuse all writes.
5. **Session transition (must refuse).** With the plugin loaded the whole time: offline race ->
   exit to menu -> host (or join). Expected: the per-second recheck flips to refuse at the
   transition and no write occurs after it; the log shows the state change and the release of any
   lever that was active offline.

Pass criteria: steps 3-5 produce zero writes; step 1 produces normal offline control. Any
deviation falsifies the guard and the signal must be revised before release.

---

## 6. Open items (for Claude)

1. Live host read of S1/S2/S4 and a remote car's S3 (section 5 step 3) - converts S1 host=1/2 and
   the S2/S4 secondary checks from DERIVED to OBSERVED.
2. 1-vs-2 semantics of the host state (dedicated vs normal) - not needed for the guard; only for
   the log wording.
3. `docs/12-globals-reference.md:108` ("bit 0 = multiplayer active") should be corrected per S2.
4. Implement `abl_may_act` in `development/mods/ai_brain_lab/dll/ai_brain_lab.c` (Claude owns the
   mod; this batch did not touch it) and add the guard-decision log.
5. `findings.jsonl` was NOT edited (batch rules); candidate findings for Claude to append:
   `ai-brain-mp-guard-session-state-3fc4` (S1 semantics + guard rule), `ai-brain-mp-flags-7c56a0-value-3-offline-and-host`
   (S2 writer/reader set + docs contradiction), `ai-brain-vehicle-mode-32c8-local-vs-network`
   (S3 writers 0/1/2/-1/3 and the AIMainUpdate gate).

## 7. Reproducible static commands (all from `f1c_ida_decompiled`, read-only)

```powershell
python tools/f1c_mod.py evidence-query findings multiplayer --json
python tools/f1c_mod.py evidence-query findings host --json
python tools/f1c_mod.py evidence-query findings client --json
python tools/f1c_mod.py evidence-query findings session --json
rg -n "3FC4" functions --glob "*.c"            # S1 writers/readers by byte offset
rg -n "16324|4081" functions --glob "*.c"      # S1 writes as dword index (this[4081])
rg -n "dword_7C56A0" functions --glob "*.c"    # S2 writer/reader census
rg -n "13000" functions --glob "*.c"           # S3 users (0x32C8)
rg -n "dword_7D0410" functions --glob "*.c"    # S4 writer/reader census
rg -n "SessionStateTransition" functions --glob "*.c"   # S3 writers
```

Key files read in full (all under `functions/`): `SessionStateCheck_00461cd0.c`,
`SessionStateCheckWrapper_00461db0.c`, `StateCheckWrapper_00461dc0.c`,
`MultiplayerVehiclePlacement_0042cf10.c`, `DriverEntrantSetupEntry_00462550.c`,
`CollisionEventNetworkBroadcast_00462940.c`, `SessionStateTransition_004ef5a0.c`,
`NetCommPlayerSlotSync_0045fc50.c`, `AIMainUpdate_00424e70.c`, `MainGameTickUpdate_0042c690.c`,
`GameMode_0046d6d0.c`, `GameMainEntry_0045b200.c`, `Game_Post_0042da30.c`,
`GamePostMainLoop_0042d8a0.c`, `SessionTypeSetter_00463f70.c`, `AIDriverCountSetup_004bfd90.c`,
`sub_463BC0_00463bc0.c`, `ScoringGateCheck_0042bc10.c`, `AutoJoinRetryLoop_004637a0.c`.

No live action, no exe modification, no writes outside `investigations/ai_brain_map/phase3/`.
