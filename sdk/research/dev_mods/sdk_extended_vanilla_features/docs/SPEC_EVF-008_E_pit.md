# SPEC_EVF-008_E — pit-related cluster (7 rows)

**Source:** ASSIGNMENT_EVF-008.md Group E
**Date:** 2026-07-28
**Caller chain:** `PitTimer_004d2e50` → `sub_40DE60_0040de60` → various dispatch functions

**IMPORTANT CORRECTION:** The assignment header says emitter = `PitRaceStatusMessageDispatcher_0041be60`. That function emits ONLY weather/temperature eids (506-513) — NONE of the Group E clips. The Group E clips share the CALLER CHAIN (`sub_40DE60 ← PitTimer`) but each has its own emitter function. I trace each below.

## Caller chain legitimacy

`sub_40DE60` is called from `PitTimer_004d2e50` (CONFIRMED: vehicle-physics tick). `sub_40DE60` touches `RevLimiter` (+0x94) and `EngineAngVel` (+0xA0) — FACT-tier vehicle-struct fields accessed during live physics. The chain IS a per-frame simulation tick, not a menu. Every function in this chain is unambiguously live.

## Per-clip analysis

### PWS_2001_131 — "AI driver engine stalled in pits" (eid 184, sdt 196)

**Verdict: MENU_TRAP**

Emitter: `PitExit_0041c0f0` (per the SHAU CONFIRMED finding). Called via `sub_40DE60` → `CarSelectionStateMachine_00434460` → `PitExit`.

`CarSelectionStateMachine` is identified in `skeletons_filled/CarSelectionStateMachine_00434460.md` as the **pre-race grid/formation-lap camera state machine**. The SHAU finding explicitly checked this: "name looked menu-like but ... identifies it as the pre-race grid/formation-lap camera state machine, a real in-session phase, not a menu — left as SHIPPED."

However, the assignment specifically asks: *"can 'engine stalled in pits' even occur during grid/formation-lap (is the AI car near a pit at that point in a race weekend?) or whether the description implies a moment this state machine can't represent."*

Analysis: During formation-lap/grid phase, AI cars are on track, not in pit boxes. An "engine stalled in pits" event requires the car to be IN the pit. While `CarSelectionStateMachine` is a legitimate in-session state (not a post-race menu), it represents a phase where cars are on the grid or formation lap — NOT in pit boxes. The description "engine stalled in pits" describes a pit-lane event that cannot occur during this state machine's active window.

This is the same structural problem as PWS_GREEN01/YELLOW04/05/06: the caller is live and the emitter code path exists, but the specific event described requires a game situation the calling context cannot supply.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_2001_131 | 196 | 184 | MENU_TRAP |

Evidence:
- SHAU CONFIRMED finding: `CarSelectionStateMachine` identified as pre-race grid/formation-lap state
- `sub_40DE60_0040de60.c` callees list: `CarSelectionStateMachine_00434460`
- `skeletons_filled/CarSelectionStateMachine_00434460.md`

### RE_OD_MONSOON — "Other driver leaves pits on Monsoons" (eid 192, sdt 204)

**Verdict: NO_KNOWN_WRITER**

eid 192 has NO literal or dynamic emitter in `emitter_index.json`. Not in `sub_41AC50`, not in `PitRaceStatusMessageDispatcher`, not in any `DriverDataTableLookupAndTimeConversion` call site found via grep of the 7,138-function corpus. The eid exists in `full_event_dump.json` (cd=1.0, score=195, blocked=false) and plays correctly on isolation-injection, but no code dispatches it.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| RE_OD_MONSOON | 204 | 192 | NO_KNOWN_WRITER |

### PWS_AIPIT11 — "AI pits to serve stop-go penalty" (eid 195, sdt 207)

**Verdict: NO_KNOWN_WRITER**

Same as above — eid 195 absent from both emitter_index sections and from all `DriverDataTableLookupAndTimeConversion` call-site grep hits.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_AIPIT11 | 207 | 195 | NO_KNOWN_WRITER |

### PWS_2001_141 — "Team positioned to secure WCC" (eid 199, sdt 212)

**Verdict: NO_KNOWN_WRITER**

eid 199 absent from emitter_index. This is a team/championship context clip — the description "Team positioned to secure WCC" suggests it may be dispatched through a constructor-name lookup (like `sub_417640`). However, `sub_417640` maps team names to event IDs in the 408-438 range, not 199. The eid 199 is in the regular PWS range — if a name-lookup emitted it, the lookup function's mapped values would need to include 199, and grep found none.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_2001_141 | 212 | 199 | NO_KNOWN_WRITER |

### PWS_2001_138 — "AI driver positioned to secure WDC" (eid 200, sdt 213)

**Verdict: NO_KNOWN_WRITER**

Same analysis as eid 199.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_2001_138 | 213 | 200 | NO_KNOWN_WRITER |

### RE_OD_LOST_7 — "Other driver lost 7th gear" (eid 209, sdt 222)

**Verdict: UNKNOWN**

eid 209 has no emitter in `emitter_index.json`. However, this eid is immediately adjacent to the `sub_41AC50` gear-loss family (eids 201-217) and the description "lost 7th gear" is mechanically identical to "lost N gear" (eids 201-206). 

Possibility: eid 209 may share `sub_41AC50`'s dispatch mechanism via a different call site that wasn't captured. The `sub_41AC50` function dispatches eids 208, 211-217 for specific a2 values, but has no case for eid 209. If eid 209 is emitted at all, it would be through a different function — possibly `sub_41AC50` with an a2 value not captured in the decompiled switch, or a sibling function that mirrors the gear-loss dispatch.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| RE_OD_LOST_7 | 222 | 209 | UNKNOWN |

### RE_OD_STUCK_7 — "Other driver stuck in 7th gear" (eid 211, sdt 224)

**Verdict: LIVE_UNPROVEN_PREDICATE**

Already covered under Group B analysis. Emitted by `sub_41AC50` with `a2=2, CurrentGear=default` (when gear value is outside 0-6 range, i.e., 7th gear ≥6). Same timer-expiry problem: `veh+0x3D68` never reaches 0 in a normal race.

**Note:** eid 211 was in the prior DERIVED finding as "RE_OD_STUCK_7 and PWS_AIMECHPROB1 have normal 15.0 cooldowns and SessionType==7 gates (currently satisfied)." The emitter IS known (`sub_41AC50`), it was isolation-tested and confirmed audible, but the upstream condition (case-2 timer expiry in `FuelConsumptionAndWear`) is the issue.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| RE_OD_STUCK_7 | 224 | 211 | LIVE_UNPROVEN_PREDICATE |

## Summary table

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| PWS_2001_131 | 196 | 184 | MENU_TRAP |
| RE_OD_MONSOON | 204 | 192 | NO_KNOWN_WRITER |
| PWS_AIPIT11 | 207 | 195 | NO_KNOWN_WRITER |
| PWS_2001_141 | 212 | 199 | NO_KNOWN_WRITER |
| PWS_2001_138 | 213 | 200 | NO_KNOWN_WRITER |
| RE_OD_LOST_7 | 222 | 209 | UNKNOWN |
| RE_OD_STUCK_7 | 224 | 211 | LIVE_UNPROVEN_PREDICATE |

## Evidence

- `sub_40DE60_0040de60.c` callees list: `PitRaceStatusMessageDispatcher`, `DisplayMessage0x151`, `DisplayMessage0x202`, `CarSelectionStateMachine`, `PositionBasedMessageDisplay`, `sub_41C440`
- `emitter_index.json`: eids 184,192,195,199,200,209 absent from both literal and dynamic
- CONFIRMED SHAU finding: carrier chain via `PitTimer` verified
- `PitRaceStatusMessageDispatcher_0041be60.c`: emits eids 506-513 only (weather/temperature), not Group E eids
- `PitExit_0041c0f0` is the emitter for eid 184 (via `CarSelectionStateMachine`)
- `sub_41AC50` emits eid 211 (see Group B spec)

## uncertainty

1. **eids 195, 199, 200 may be reachable through `DriverNameIDLookup`** — the dynamic name-to-eid mapping in `DriverNameIDLookup_00415d90` can resolve to eids in this range for specific driver/context combinations. An exhaustive scan of all `DriverNameIDLookup` output values for all 2001-season drivers and all `(a3,a4,a5)` context flags would confirm or rule this out. The existing mapping table in CLAUDE.md only covers a few example drivers.

2. **`PitRaceStatusMessageDispatcher` mismatch:** the assignment grouped these clips under this function, but the function emits unrelated eids (506-513). The actual emitters for most Group E clips are unidentified. The shared factor is the `sub_40DE60 ← PitTimer` caller chain, which IS live — but the dispatch functions within that chain that would handle eids 192,195,199,200,209 are not `PitRaceStatusMessageDispatcher`.

3. **PWS_2001_131 and CarSelectionStateMachine:** the SHAU finding accepted this as a live path, but the description-vs-context mismatch ("engine stalled in pits" during grid/formation-lap) may mean the clip's intended trigger condition cannot be satisfied by the state machine that calls it. This is DERIVED from the state machine's known phase (pre-race grid/formation lap, not pit-stop window) — live verification would require confirming whether AI cars can be "in pits" during the formation lap.
