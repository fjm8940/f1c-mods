# H3 mechanical design (ERE batch H3_mechanical)

Status: design only (static research). No game run, no live test, nothing installed. Trust tiers per HAIKU_BRIEF: FACT > CONFIRMED > DERIVED > GUESS.
Machine-readable copy: `data/design/H3_mechanical.json` (43 rows, schema ere.design.v1).

## Counts by technique

- owned_by_other_mod: 34
- hook_native: 6
- skip: 3

## One row per clip

| sdt | clip | PWS eid | says | technique | trigger (one line) | blocker |
|---|---|---|---|---|---|---|
| 74 | PWS_MECHPROB16 | 75 | Damaged suspension - avoid kerbs | owned_by_other_mod | suspension_failure stage reaches damaged state (steering-geometry bend via game's SteeringGeometryUpdate_00442060); queued once per arm | owned: mechanical_failures |
| 75 | PWS_MECHPROB14 | 76 | Electronics malfunction | owned_by_other_mod | seeded 10% roll per race/quali session + constructed lifetime timer; line once per session when electronics fault activates; fault = fuel-starvatio... | owned: mechanical_failures |
| 77 | PWS_MECHPROB06 | 78 | Front Left puncture | hook_native | per-wheel tread (this+0x394 = tread 0.75 threshold) crosses downward through 0.75 at corner 0 (DERIVED FL=0 FR=1 RL=2 RR=3, EMITTER_SPECS). Native ... | no_emitter |
| 78 | PWS_MECHPROB07 | 79 | Front Right puncture | hook_native | per-wheel tread (this+0x394 = tread 0.75 threshold) crosses downward through 0.75 at corner 1 (DERIVED FL=0 FR=1 RL=2 RR=3, EMITTER_SPECS). Native ... | no_emitter |
| 79 | PWS_MECHPROB20 | 80 | Lost 1st gear | owned_by_other_mod | gear_failure lost-gear fault on gear 1 (fault latch g_announced; say_eid(EID_LOST(1)) at gear_failure.c:1586) | owned: mechanical_failures |
| 80 | PWS_MECHPROB21 | 81 | Lost 2nd gear | owned_by_other_mod | gear_failure lost-gear fault on gear 2 (fault latch g_announced; say_eid(EID_LOST(2)) at gear_failure.c:1586) | owned: mechanical_failures |
| 81 | PWS_MECHPROB22 | 82 | Lost 3rd gear | owned_by_other_mod | gear_failure lost-gear fault on gear 3 (fault latch g_announced; say_eid(EID_LOST(3)) at gear_failure.c:1586) | owned: mechanical_failures |
| 82 | PWS_MECHPROB23 | 83 | Lost 4th gear | owned_by_other_mod | gear_failure lost-gear fault on gear 4 (fault latch g_announced; say_eid(EID_LOST(4)) at gear_failure.c:1586) | owned: mechanical_failures |
| 83 | PWS_MECHPROB24 | 84 | Lost 5th gear | owned_by_other_mod | gear_failure lost-gear fault on gear 5 (fault latch g_announced; say_eid(EID_LOST(5)) at gear_failure.c:1586) | owned: mechanical_failures |
| 84 | PWS_MECHPROB25 | 85 | Lost 6th gear | owned_by_other_mod | gear_failure lost-gear fault on gear 6 (fault latch g_announced; say_eid(EID_LOST(6)) at gear_failure.c:1586) | owned: mechanical_failures |
| 85 | PWS_MECHPROB26 | 86 | Lost 7th gear | owned_by_other_mod | gear_failure lost-gear fault on gear 7 (fault latch g_announced; say_eid(EID_LOST(7)) at gear_failure.c:1586) | owned: mechanical_failures |
| 86 | PWS_MECHPROB27 | 87 | Stuck in 1st gear | owned_by_other_mod | gear_failure stuck-gear state on gear 1 (say_eid(EID_STUCK(1)) at gear_failure.c:1584) | owned: mechanical_failures |
| 87 | PWS_MECHPROB28 | 88 | Stuck in 2nd gear | owned_by_other_mod | gear_failure stuck-gear state on gear 2 (say_eid(EID_STUCK(2)) at gear_failure.c:1584) | owned: mechanical_failures |
| 88 | PWS_MECHPROB29 | 89 | Stuck in 3rd gear | owned_by_other_mod | gear_failure stuck-gear state on gear 3 (say_eid(EID_STUCK(3)) at gear_failure.c:1584) | owned: mechanical_failures |
| 89 | PWS_MECHPROB30 | 90 | Stuck in 4th gear | owned_by_other_mod | gear_failure stuck-gear state on gear 4 (say_eid(EID_STUCK(4)) at gear_failure.c:1584) | owned: mechanical_failures |
| 90 | PWS_MECHPROB31 | 91 | Stuck in 5th gear | owned_by_other_mod | gear_failure stuck-gear state on gear 5 (say_eid(EID_STUCK(5)) at gear_failure.c:1584) | owned: mechanical_failures |
| 91 | PWS_MECHPROB32 | 92 | Stuck in 6th gear | owned_by_other_mod | gear_failure stuck-gear state on gear 6 (say_eid(EID_STUCK(6)) at gear_failure.c:1584) | owned: mechanical_failures |
| 92 | PWS_MECHPROB33 | 93 | Stuck in 7th gear | owned_by_other_mod | gear_failure stuck-gear state on gear 7 (say_eid(EID_STUCK(7)) at gear_failure.c:1584) | owned: mechanical_failures |
| 101 | PWS_MECHPROB08 | 102 | Rear Left puncture | hook_native | per-wheel tread (this+0x394 = tread 0.75 threshold) crosses downward through 0.75 at corner 2 (DERIVED FL=0 FR=1 RL=2 RR=3, EMITTER_SPECS). Native ... | no_emitter |
| 102 | PWS_MECHPROB09 | 103 | Rear Right puncture | hook_native | per-wheel tread (this+0x394 = tread 0.75 threshold) crosses downward through 0.75 at corner 3 (DERIVED FL=0 FR=1 RL=2 RR=3, EMITTER_SPECS). Native ... | no_emitter |
| 111 | RE_PD_STALLED | 112 | Engine stalled - T-car prepared | skip | UNKNOWN: no spare-car (T-car) mechanic in game code found; owner idea (teleport to pit garage, restart) is UNPROVEN | cooldown_flt_max |
| 112 | RE_PD_STALLED_T | 113 | T-car also stalled - session over | skip | UNKNOWN: depends on 111 and on a second stall. Should not be a second retire line (117-119 owned by others). | cooldown_flt_max |
| 118 | PWS_RETIRE02 | 117 | Player retired (generic 1) | owned_by_other_mod | FinishState veh+0x4064 transitions to 2 and ReasonCode veh+0x4068 is not 1 or 7 (native emitter branch) | owned: player_retire_commentary |
| 120 | PWS_RETIRE01 | 118 | Player engine stall | owned_by_other_mod | FinishState 0->2 with ReasonCode veh+0x4068 == 7. No native writer of ReasonCode 7 found: writers are 1 (engine, FuelConsumptionAndWear_004ed960.c:... | owned: player_retire_commentary |
| 121 | PWS_RETIRE04 | 119 | Player blown engine | owned_by_other_mod | MF engine death (ReasonCode 1, ENG_RUNNING 0) or native ReasonCode 1 terminal state | owned: mechanical_failures |
| 134 | PWS_MECHANWARN0 | 130 | Meatball flag - must pit (MF: 'pit on this lap, marshals ... | owned_by_other_mod | MF engine symptom with a critical-fault state (messaging audit: critical only when engine dies soon if driver carries on; engine_failure.c:459-462) | owned: mechanical_failures |
| 214 | PWS_AIMECHPROB0 | 201 | <driver> has lost 1st gear (AI) | owned_by_other_mod | AI gearbox model terminal LOST fault on gear 1 (gbx_sim_pump.h:412-415 -> gbx_eid_lost(1)) | owned: mechanical_failures |
| 215 | PWS_AIMECHPROB0 | 202 | <driver> has lost 2nd gear (AI) | owned_by_other_mod | AI gearbox model terminal LOST fault on gear 2 (gbx_sim_pump.h:412-415 -> gbx_eid_lost(2)) | owned: mechanical_failures |
| 216 | PWS_AIMECHPROB1 | 203 | <driver> has lost 3rd gear (AI) | owned_by_other_mod | AI gearbox model terminal LOST fault on gear 3 (gbx_sim_pump.h:412-415 -> gbx_eid_lost(3)) | owned: mechanical_failures |
| 217 | PWS_AIMECHPROB1 | 204 | <driver> has lost 4th gear (AI) | owned_by_other_mod | AI gearbox model terminal LOST fault on gear 4 (gbx_sim_pump.h:412-415 -> gbx_eid_lost(4)) | owned: mechanical_failures |
| 218 | PWS_AIMECHPROB1 | 205 | <driver> has lost 5th gear (AI) | owned_by_other_mod | AI gearbox model terminal LOST fault on gear 5 (gbx_sim_pump.h:412-415 -> gbx_eid_lost(5)) | owned: mechanical_failures |
| 219 | PWS_AIMECHPROB1 | 206 | <driver> has lost 6th gear (AI) | owned_by_other_mod | AI gearbox model terminal LOST fault on gear 6 (gbx_sim_pump.h:412-415 -> gbx_eid_lost(6)) | owned: mechanical_failures |
| 220 | PWS_AIMECHPROB1 | 207 | Periodically slowing down (AI) | skip | UNKNOWN: no real periodic-slowdown state proven. MF AI symptoms spec (AI_SYMPTOMS_AND_REACTIONS_SPEC_2026-10-01) covers power loss and smoke, not a... | no_emitter |
| 221 | PWS_AIMECHPROB1 | 208 | Smoke coming from engine (AI) | hook_native | MF AI engine smoke onset (real AI oil/blow-by smoke state from MF AI engine model, AI symptoms spec). ERE/MF calls native sub_41AC50(aiVehicle, 0) ... | gate_never_true |
| 222 | RE_OD_LOST_7 | 209 | Other driver lost 7th gear | owned_by_other_mod | AI gearbox terminal LOST on gear 7 (gbx_eid_lost(7) = 209, gbx_sim_core.h:145) | owned: mechanical_failures |
| 223 | RE_OD_PUNCTURE | 210 | Other driver has a puncture | hook_native | same observer as PWS_MECHPROB06-09 but on an AI car: per-wheel tread crosses 0.75 and the event is a real puncture (gate as for player) | no_emitter |
| 224 | RE_OD_STUCK_7 | 211 | Other driver stuck in 7th gear | owned_by_other_mod | AI gearbox terminal STUCK/ACTUATOR on gear 7 (gbx_eid_stuck(7) = 211) | owned: mechanical_failures |
| 225 | PWS_AIMECHPROB0 | 212 | <driver> is stuck in 1st gear (AI) | owned_by_other_mod | AI gearbox terminal STUCK/ACTUATOR while gear is 1 (gbx_eid_stuck(1) = 212, gbx_sim_core.h:144) | owned: mechanical_failures |
| 226 | PWS_AIMECHPROB0 | 213 | <driver> is stuck in 2nd gear (AI) | owned_by_other_mod | AI gearbox terminal STUCK/ACTUATOR while gear is 2 (gbx_eid_stuck(2) = 213, gbx_sim_core.h:144) | owned: mechanical_failures |
| 227 | PWS_AIMECHPROB0 | 214 | <driver> is stuck in 3rd gear (AI) | owned_by_other_mod | AI gearbox terminal STUCK/ACTUATOR while gear is 3 (gbx_eid_stuck(3) = 214, gbx_sim_core.h:144) | owned: mechanical_failures |
| 228 | PWS_AIMECHPROB0 | 215 | <driver> is stuck in 4th gear (AI) | owned_by_other_mod | AI gearbox terminal STUCK/ACTUATOR while gear is 4 (gbx_eid_stuck(4) = 215, gbx_sim_core.h:144) | owned: mechanical_failures |
| 229 | PWS_AIMECHPROB0 | 216 | <driver> is stuck in 5th gear (AI) | owned_by_other_mod | AI gearbox terminal STUCK/ACTUATOR while gear is 5 (gbx_eid_stuck(5) = 216, gbx_sim_core.h:144) | owned: mechanical_failures |
| 230 | PWS_AIMECHPROB0 | 217 | <driver> is stuck in 6th gear (AI) | owned_by_other_mod | AI gearbox terminal STUCK/ACTUATOR while gear is 6 (gbx_eid_stuck(6) = 217, gbx_sim_core.h:144) | owned: mechanical_failures |

Note: PWS_AIMECHPROB0 / PWS_AIMECHPROB1 are truncated SDT names. Resolved by eid (doc56 'Sources' rule), not by name.

## Ownership details (file:line)

- sdt 74 PWS_MECHPROB16 eid 75: mechanical_failures development/mods/mechanical_failures/dll/suspension_failure.c:87 (EID_MECHPROB16), :283-293, :332; shipped per development/mods/mechanical_failures/README.md:110
- sdt 75 PWS_MECHPROB14 eid 76: mechanical_failures development/mods/mechanical_failures/dll/electronics_failure.c:117 (EID_ELEC 76), :386 radio_queue, :751 radio_say; dev DLL, README.md:112 'live validation pending'
- sdt 79 PWS_MECHPROB20 eid 80: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 80 PWS_MECHPROB21 eid 81: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 81 PWS_MECHPROB22 eid 82: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 82 PWS_MECHPROB23 eid 83: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 83 PWS_MECHPROB24 eid 84: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 84 PWS_MECHPROB25 eid 85: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 85 PWS_MECHPROB26 eid 86: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 86 PWS_MECHPROB27 eid 87: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 87 PWS_MECHPROB28 eid 88: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 88 PWS_MECHPROB29 eid 89: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 89 PWS_MECHPROB30 eid 90: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 90 PWS_MECHPROB31 eid 91: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 91 PWS_MECHPROB32 eid 92: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 92 PWS_MECHPROB33 eid 93: mechanical_failures development/mods/mechanical_failures/dll/gear_failure.c:96-97 (EID_LOST/EID_STUCK), :1409 unblock of 14 FLT_MAX cooldowns, :1584-1586 say_eid; gear_failure.dll shipped (development/mods/mechanical_failures/README.md:109)
- sdt 118 PWS_RETIRE02 eid 117: player_retire_commentary development/mods/player_retire_commentary/retire_commentary.c:62 (EID_RETIRE02), :24-25 mapping; also pitstop_modding development/mods/pitstop_modding/live/live_client.cpp:1443 voice(c,117) on garage retire
- sdt 120 PWS_RETIRE01 eid 118: player_retire_commentary retire_commentary.c:61 (EID_RETIRE01) and :24 (ReasonCode 7 -> 118)
- sdt 121 PWS_RETIRE04 eid 119: mechanical_failures development/mods/mechanical_failures/dll/engine_failure.c:527 (EID_RETIRE04), :570 radio_req(MFR_P_DEATH), :591; ALSO player_retire_commentary retire_commentary.c:60 (ReasonCode 1 -> 119)
- sdt 134 PWS_MECHANWARN0 eid 130: mechanical_failures development/mods/mechanical_failures/dll/engine_failure.c:454 (EID_MECHWARN 130), say_after callers :2889, :2986, :5120, :5327, :5384, :5745, :5948
- sdt 214 PWS_AIMECHPROB0 eid 201: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 215 PWS_AIMECHPROB0 eid 202: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 216 PWS_AIMECHPROB1 eid 203: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 217 PWS_AIMECHPROB1 eid 204: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 218 PWS_AIMECHPROB1 eid 205: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 219 PWS_AIMECHPROB1 eid 206: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 222 RE_OD_LOST_7 eid 209: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 224 RE_OD_STUCK_7 eid 211: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 225 PWS_AIMECHPROB0 eid 212: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 226 PWS_AIMECHPROB0 eid 213: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 227 PWS_AIMECHPROB0 eid 214: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 228 PWS_AIMECHPROB0 eid 215: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 229 PWS_AIMECHPROB0 eid 216: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)
- sdt 230 PWS_AIMECHPROB0 eid 217: mechanical_failures DEV ONLY development/mods/mechanical_failures/dll/gbx_sim_core.h:125-155 (gbx_say, formatter 0x4144F0), development/mods/mechanical_failures/dll/gbx_sim_pump.h:412-418 (terminal LOST/STUCK/ACTUATOR). NOT in v1.0.7 release; G3 'not live-tested' (development/mods/mechanical_failures/README.md:5)

## Shared state/hooks this batch needs

- Puncture observer (rows sdt 77, 78, 101, 102, 223): hook inside `sub_438130` (VehicleDynamicsForceAccumulatorIntegration_00438130.c:154-165) or the wheel loop at `VehiclePhysicsIntegrationStep` 0x00441650 (EMITTER_SPECS.md). Reads per-wheel tread at this+0x394 (heap object; NOT veh+0x363C). Needs a gate that separates a real puncture from ordinary wear-out.
- AI smoke (sdt 221): one call of native `sub_41AC50(aiVehicle, 0)` at MF AI smoke onset, race only (SessionType 0x7C56A4 == 7). Driver id = veh+0x489C.
- Commentary queue (MF pattern, shared): record init 0x4144D0, formatter 0x4144F0, InsertDriverTableEntry 0x418560 on controller 0x5EA618; cooldown table at 0x5BF9A0 + eid*0x14 + 0x0C.
- Retire path: a single owner for eid 117-119 per engine/garage death (conflict list above).

## Open questions

1. Is the 0.75 tread crossing reached by impact or only by wear? (life 0 = (tread-0.75)*4, findings). Decides whether a puncture line can be real.
2. Which code writes ReasonCode 7 (retire 118 'engine stall')? Enum in engine_failure.c:677 says Clutch.
3. Does the spare-car (T-car) exist in team parameters? Blocks rows 111-112.
4. Does F1CUI play eid 210 itself, or only create AI punctures?
5. Are player_retire_commentary and MF both loaded? Double 119 and 117 risk (rows 118, 121).
6. Owner ear-check for MF gearbox lines and AI gear lines (not in v1.0.7; dev only).

## Prior-art contradictions found

- Triage (sdt 74) says damage.ini has suspension values; SUSPENSION_DAMAGE.md:262 says it has no suspension section.
- SPEC_EVF-008_B says eids 201-206 have no writer; true for native code, but MF writes them itself (gbx_sim_core.h:147-155). Not a conflict for the design, but EVF's 'NO_KNOWN_WRITER' is now stale for the dev build.
- Triage (sdt 77) says 0x4F199E is the puncture lead; findings say 0x4F199E is grip physics and the tread trigger is per-wheel this+0x394, not veh+0x363C.
- sub_41AC50 a2==3 emits eid 220 (AIHIT08 suspension damage), not 207 'periodically slowing'.
- ReasonCode 7 label: retire_commentary says engine stall; engine_failure enum says Clutch.
- Assignment note for sdt 222 cites SPEC_EVF-008_E_pit.md; the gear family is SPEC_EVF-008_B.

## Cross-batch notes (not in H3)

- sdt 76 PWS_MECHPROB15 (eid 77, engine overheating) is not in H3. Owned: native sub_41B190 (functions/sub_41B190_0041b190.c:7) and MF engine_failure.c:3136. Shipped per doc56.
- sdt 233 PWS_AIHIT08 (eid 220) is written by native sub_41AC50 a2==3 (AI suspension damage). Belongs to the incidents batch.

