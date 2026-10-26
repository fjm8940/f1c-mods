# H6 weather and tyres: design summary

Batch H6_weather_tyres, 29 clips. Static design only, no live game work. Machine-readable rows: `data/design/H6_weather_tyres.json`.

Techniques: game_state_emitter 19, owned_by_other_mod 8, hook_native 2, skip 0. Owner goal is all lines; no clip is skipped. Forecast and overcast lines use the weather timeline (sub_4FE350).

| Clip | Says | Technique | Trigger (one line) | Blocker |
|---|---|---|---|---|
| PWS_2001_22 | Rain expected, make run ASAP | game_state_emitter | dry now, timeline shows wet (>=0.33) within 600 s | session clock address UNKNOWN; window proposed |
| PWS_2001_24 | Rain expected to ease | game_state_emitter | wet now (>=0.33), timeline drops below 0.15 within 600 s | as above |
| PWS_2001_25 | Rain expected to continue | game_state_emitter | wet now, timeline stays >=0.33 for 600 s | as above |
| PWS_OIL01 | Oil on track S1 | game_state_emitter | AI ReasonCode 0x4068 to 9, car in sector 1 | sector source UNKNOWN; ReasonCode 9 source to confirm |
| PWS_OIL02 | Oil on track S2 | game_state_emitter | same, sector 2 | same |
| PWS_OIL03 | Oil on track S3 | game_state_emitter | same, sector 3 | same |
| RE_OD_INTER | Other driver leaves pits on Inters | game_state_emitter | AI pittingStatus to 4 and compound Intermediate | 0x458C current vs next; id mapping |
| RE_OD_MONSOON | Other driver leaves pits on Monsoons | game_state_emitter | as INTER, compound Monsoon | Monsoon id 1 vs 6 |
| RE_OD_SLICKS | Other driver leaves pits on Slicks | game_state_emitter | AI exit on Hard/Soft while wetness in band | wetness band and cooldown are proposals |
| RE_OD_WET | Other driver leaves pits on Wets | game_state_emitter | AI exit on Wet (id 5) | id to confirm; TV 509 already native |
| PWS_AITYRES02 | AI severe blistering | hook_native | AI tread crosses 0.85 (per corner, once) | no AI emitter; AI wear rate unknown |
| PWS_AITYRES01 | AI light blistering | hook_native | AI tread crosses 0.925 (per corner, once) | same |
| PWS_2001_221 | Rain starts, monsoon likely | game_state_emitter | OnLineWetness up through 0.66, race | 0.66 is an AI tyre constant; writer unknown |
| PWS_2001_222 | Rain eases off | game_state_emitter | OnLineWetness down through 0.15 | 0.15 uncited (code uses 0.2) |
| PWS_2001_218 | Overcast, possible rain | game_state_emitter | dry now, timeline shows any wetness >=0.01 within 30 min | no sky state in game; wording caveat for owner |
| PWS_2001_220 | Rain starts, short shower | game_state_emitter | OnLineWetness up through 0.33 | shower wording is a forecast |
| PWS_2001_311 | Fastest AI on Inters | game_state_emitter | quali fastest lap, AI compound Intermediate, replaces pole line | fitted vs planned compound |
| PWS_2001_310 | Fastest AI on Slicks | game_state_emitter | as above, Hard or Soft | owner rule on pole line unclear |
| PWS_2001_312 | Fastest AI on Wets | game_state_emitter | as above, Wet | as above |
| PWS_2001_308 | You are being chased | game_state_emitter | car behind player is AI, gap below threshold, closing | gap threshold; team-order wording not supported |
| RE_OD_FASTEST_M | Other driver fastest on Monsoon | game_state_emitter | as 311, Monsoon | Monsoon id 1 vs 6 |
| RE_PD_FL_BAD_BL | FL severe blistering | owned_by_other_mod | player FL tread through 0.85 | native emitter exists; never heard because 0.85 is above normal wear. Fix (threshold patch) already registered by tyre_blister_commentary; ERE adds no code |
| RE_PD_FL_BLIST | FL light blistering | owned_by_other_mod | player FL tread through 0.925 | same |
| RE_PD_FR_BAD_BL | FR severe | owned_by_other_mod | FR tread through 0.85 | same |
| RE_PD_FR_BLIST | FR light | owned_by_other_mod | FR tread through 0.925 | same |
| RE_PD_RL_BAD_BL | RL severe | owned_by_other_mod | RL tread through 0.85 | same, rear wear rate |
| RE_PD_RL_BLIST | RL light | owned_by_other_mod | RL tread through 0.925 | same |
| RE_PD_RR_BAD_BL | RR severe | owned_by_other_mod | RR tread through 0.85 | same |
| RE_PD_RR_BLIST | RR light | owned_by_other_mod | RR tread through 0.925 | same |

## Shared state and hooks this batch needs

- Wetness: `0x8AFEE8` (OnLineWetness) and `0x8AFEEC` (OffLineWetness), CONFIRMED. Writer during a race UNKNOWN. `0x7CF3A4` is not wetness.
- Ambient `0x8AFEF0`, track temp `0x8AFEF4` (DERIVED, weather.txt parser sub_4FE350).
- Tread per corner: `veh+0x3924` FL, `0x382C` FR, `0x3734` RL, `0x363C` RR (CONFIRMED). Corner order 0=FL..3=RR is DERIVED from switch order.
- Compound index `veh+0x458C` (CONFIRMED offset, meaning UNKNOWN).
- AI flag `veh+0x32C8`. pittingStatus `veh+0x3450`. Position `veh+0x4060`. Subject id `veh+0x489C` (brief).
- Fastest-lap standings: `0x7D2DB0`, entry +408 (DERIVED).
- Hooks: AI blister dispatch (hook_native) in `sub_41AA10` / `sub_41AB30`; new per-tick observer for weather and tread crossings (MainGameTickUpdate or post-loop in VehiclePhysicsIntegrationStep).
- Engine failure: `veh+0x4068` (ReasonCode, written 9 at EngineStressSimulation_0043ef20.c:76).

## Contradictions found (prior art vs code)

1. **Blister light is a crossing, not a band.** `docs/SPEC_EVF-008_A_blister.md` says light fires while 0.85 < tread <= 0.925. The decompile (`VehicleDynamicsForceAccumulatorIntegration_00438130.c:144-146`) fires once when tread crosses 0.925 downward. Severe at 0.85 matches.
2. **Wetness thresholds are AI tyre-choice constants.** The weather_track thresholds 0.01, 0.33, 0.66 come from `AIPushLap_0042a180.c:44,49,61`, not from any emitter. 0.15 for "easing" is not in the code (AIPushLap uses 0.2 at line 54).
3. **Compound id map.** EMITTER_SPECS says 1=Monsoon, 2/3=Slick, 4=Inter, 5=Wet. My reading of `sub_4A4830` gives Hard=2, Soft=3, Inter=4, Wet=5, Monsoon=6, unrecognised=1. The 2026-07-28 CONFIRMED finding says compound indices are per-track `.tbc` lists, so no fixed enum may apply. Live check needed.
4. **Native weather dispatcher.** `PitRaceStatusMessageDispatcher` (sub_41BE60) writes TV eids 506-513, including 509 (WET) and 511 (MONSOON). EVF-008_E says RE_OD_MONSOON has no known writer. That holds for PWS 192 only. The dispatcher's meaning (switch on sub_4FF130) is not identified, so the TV lines may not be "other driver leaves pits".
5. **Engine failure source.** EMITTER_SPECS says ReasonCode 9 is the engine failure. mechanical_failures uses different AI ReasonCodes (1/5/2/4). Confirm which path sets 9 for player and AI.
6. **Weather may never change mid-race.** sub_4FE350 builds a per-minute weather timeline from weather.txt (FACT: parser runs at Game_Restart; node layout read from code). The runtime link that moves timeline values into 0x8AFEE8 is not traced. If it does not change, the rain lines never fire.
7. **AI blisters may already speak player lines.** AISessionPhaseCounter and PitStateSlotActivation have no player gate, so AI tread crossings may already queue player eids 94-105. DERIVED from code only; needs an isolation test.

## Shared with other mods

- `development/mods/tyre_blister_commentary`: patches shared threshold bytes `0x59E6E8` (0.7 to 0.9) and `0x59E8C8` (0.4 to 0.6). Affects all 8 RE_PD clips. ERE must not patch these bytes again.
- `development/mods/mechanical_failures`: its own AI engine failure model. Conflict on oil trigger (item 5).

## Open questions

- Session clock global (t_now) for the forecast lines: not located.
- Does the weather timeline drive 0x8AFEE8 during a race? Runtime consumer not traced. If not, rain-starts and forecast lines have nothing to read.
- Overcast wording (no sky state in the game).
- Current versus next compound at `veh+0x458C`.
- Sector per car for oil lines (.aiw XSECTOR polygons not mapped).
- Gap threshold for the chase line; team identity for team-order wording.
- Whether the owner accepts a shorter rain-starts line without the monsoon and shower forecast words.
- Live tests (none run): wetness trace in a rain race; AI tread in a long race; monsoon id; corner order; pit-exit compound.

## Fix status for the 8 RE_PD clips

The native emitters (PitStateSlotActivation case 0-3 for severe, AISessionPhaseCounter case 0-3 for light) exist and are unblocked (cooldown 15 s). They are silent because the tread threshold is too high for normal wear. The fix is the threshold patch already in tyre_blister_commentary (0x59E6E8, 0x59E8C8). ERE adds no code. Live proof is still missing; the line was reverted on 2026-07-28.
