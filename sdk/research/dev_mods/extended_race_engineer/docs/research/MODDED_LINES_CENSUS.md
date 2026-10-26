# Modded lines census (ERE Batch H0)

Generated 2026-10-18. Read-only census of first-party mods that already make a race-engineer line (eid 1..290) play, unblock a cooldown, or carry one through another eid. Machine-readable output: `data/modded_lines.json` (schema ere.modded_lines.v1).

**Trust:** static code only. Every live_verified cell cites a prior record; nothing was run in game, no builds, no memory tools. Clip and says come from `f1c_ida_decompiled/docs/56-voice-line-reference-map.md` (doc56 names are truncated to 15 characters in places). progress = `data/clip_assignments.json` (from EVF progress.json).

## Summary

| mod | pairs | release status |
| --- | --- | --- |
| pitstop_modding | 20 | released |
| pit_exit_commentary | 3 | dev |
| mechanical_failures | 55 | released, dev |
| red_flag | 1 | released |
| full_safety_car | 1 | released |
| player_damage_commentary | 6 | dev |
| extended_damage_commentary | 6 | released |
| player_retire_commentary | 3 | candidate |
| player_dq_commentary | 1 | dev |
| traffic_density_commentary | 3 | dev |
| tyre_blister_commentary | 2 | candidate |

Total pairs: 101. Distinct eids: 90.

## Table by mod

### pitstop_modding

Version: 1.1.4 (RELEASE_REGISTRY 2026-10-11)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 51 | PWS_PIT06 | Limiter On | dll_emit | player pit entry: pittingStatus 0 or 1 -> 2 (limiter on) | released | unknown (no in-game record for this eid in this census) |
| 53 | PWS_PIT08 | Brake (pit procedure) | dll_emit | player box approach: pittingStatus becomes 3 from a non-3 state (Brake) | released | unknown (no in-game record for this eid in this census) |
| 45 | PWS_PIT10 | Go (pit procedure) | dll_emit | player box release: pittingStatus 3 -> 4 (Go) | released | unknown (no in-game record for this eid in this census) |
| 43 | PWS_PIT12 | Limiter Off - clear of pit lane | dll_emit | player pit exit: pittingStatus 4 -> 0 within 60 s of Go (limiter off) | released | unknown (no in-game record for this eid in this census) |
| 54 | PWS_2001_80 | Engine stalled - will get refired | dll_emit | player box stop: stall variant rolled in pick_player, fired from box entry | released | unknown (no in-game record for this eid in this census) |
| 56 | PWS_2001_79 | Fuel hose issue - slow refuel | dll_emit | player box stop: refuel or fuel-hose variant (roll), fired from box entry | released | unknown (no in-game record for this eid in this census) |
| 61 | PWS_2001_76 | Wheel gun failure | dll_emit | player box stop: wheel-gun failure armed (state machine), corner-dependent slow lines | released | unknown (no in-game record for this eid in this census) |
| 63 | PWS_2001_72 | Slow Front Left tire change | dll_emit | player box stop: slow Front Left tyre change (wheel-gun corner) | released | unknown (no in-game record for this eid in this census) |
| 65 | PWS_2001_73 | Slow Front Right tire change | dll_emit | player box stop: slow Front Right tyre change (wheel-gun corner) | released | unknown (no in-game record for this eid in this census) |
| 68 | PWS_2001_74 | Slow Rear Left tire change | dll_emit | player box stop: slow Rear Left tyre change (wheel-gun corner) | released | unknown (no in-game record for this eid in this census) |
| 70 | PWS_2001_75 | Slow Rear Right tire change | dll_emit | player box stop: slow Rear Right tyre change (wheel-gun corner) | released | unknown (no in-game record for this eid in this census) |
| 66 | PWS_2001_77 | Wheel is jammed | dll_emit | player box stop: wheel jammed (terminal shape of the wheel-gun state machine) | released | unknown (no in-game record for this eid in this census) |
| 117 | PWS_RETIRE02 | Player retired (generic 1) | dll_emit | player terminal wheel jam: fire(EID_RETIRE) after the jam line (retire to pits path) | released | unknown (117 also heard via player_retire_commentary, see that entry) |
| 183 | PWS_MISC30 | AI has pit stop issue | dll_emit | AI box stop: generic AI pit failure roll (8%) fired at AI box entry | released | unknown (no in-game record for this eid in this census) |
| 184 | PWS_2001_131 | AI driver engine stalled in pits | dll_emit | AI box stop: AI engine stall roll (3%) at AI box entry | released | unknown (no in-game record for this eid in this census) |
| 185 | PWS_2001_132 | AI driver jammed fuel hose | dll_emit | AI box stop: AI fuel-hose roll (6%) at AI box entry | released | unknown (no in-game record for this eid in this census) |
| 186 | PWS_2001_133 | AI driver stuck Front Left wheel | dll_emit | AI box stop: AI Front Left wheel stuck (4% per corner roll) | released | unknown (no in-game record for this eid in this census) |
| 187 | PWS_2001_134 | AI driver stuck Front Right wheel | dll_emit | AI box stop: AI Front Right wheel stuck (4% per corner roll) | released | unknown (no in-game record for this eid in this census) |
| 188 | PWS_2001_135 | AI driver stuck Rear Left wheel | dll_emit | AI box stop: AI Rear Left wheel stuck (4% per corner roll) | released | unknown (no in-game record for this eid in this census) |
| 189 | PWS_2001_136 | AI driver stuck Rear Right wheel | dll_emit | AI box stop: AI Rear Right wheel stuck (4% per corner roll) | released | unknown (no in-game record for this eid in this census) |

### pit_exit_commentary

Version: dev (no registry row; FEATURE.md:7 RESTORED)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 51 | PWS_PIT06 | Limiter On | dll_emit | pittingStatus 1 -> 2 edge (same trigger as pitstop_modding) | dev | yes (pit_exit_commentary/FEATURE.md:7 all three heard in-game 2026-07-28) |
| 45 | PWS_PIT10 | Go (pit procedure) | dll_emit | pittingStatus 3 -> 4 edge | dev | yes (pit_exit_commentary/FEATURE.md:7) |
| 43 | PWS_PIT12 | Limiter Off - clear of pit lane | dll_emit | pittingStatus 4 -> 0 edge | dev | yes (pit_exit_commentary/FEATURE.md:7) |

### mechanical_failures

Version: 1.0.7 public (RELEASE_REGISTRY 2026-10-12); dev dll/

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 49 | PWS_PIT01 | Pit on this lap | dll_emit | critical pit call: commentary_say(EID_PITCALL) on engine/water/fuel critical state; EID_PIT01 cooling call-in | released | unknown (MF owner live checks pending per project record) |
| 130 | PWS_MECHANWARN0 | Meatball flag - must pit | dll_emit | marshals warning 5-30 s after smoke, exhaust or blow-by (say_after EID_MECHWARN) | released | unknown (MF owner live checks pending per project record) |
| 77 | PWS_MECHPROB15 | Engine overheating | dll_emit | engine or water overheating (radio_req EID_OVERHEAT / EID_OVERHEAT_W / EID_OVERHEAT_OIL); row 77 is also the parity reference row | released | unknown (MF owner live checks pending per project record) |
| 119 | PWS_RETIRE04 | Player blown engine | dll_emit | player engine blown: radio_req(EID_RETIRE04) on death | released | unknown (MF owner live checks pending per project record) |
| 75 | PWS_MECHPROB16 | Damaged suspension - avoid kerbs | dll_emit | player suspension damage: commentary_queue(EID_MECHPROB16) | dev | unknown |
| 76 | PWS_MECHPROB14 | Electronics malfunction | dll_emit | player electronics malfunction: radio_queue(EID_ELEC) | dev | unknown |
| 80 | PWS_MECHPROB20 | Lost 1st gear | cooldown_unblock | player gearbox LOST 1st gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30, isolation-injected) |
| 87 | PWS_MECHPROB27 | Stuck in 1st gear | cooldown_unblock | player gearbox STUCK in 1st gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30) |
| 81 | PWS_MECHPROB21 | Lost 2nd gear | cooldown_unblock | player gearbox LOST 2nd gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30, isolation-injected) |
| 88 | PWS_MECHPROB28 | Stuck in 2nd gear | cooldown_unblock | player gearbox STUCK in 2nd gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30) |
| 82 | PWS_MECHPROB22 | Lost 3rd gear | cooldown_unblock | player gearbox LOST 3rd gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30, isolation-injected) |
| 89 | PWS_MECHPROB29 | Stuck in 3rd gear | cooldown_unblock | player gearbox STUCK in 3rd gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30) |
| 83 | PWS_MECHPROB23 | Lost 4th gear | cooldown_unblock | player gearbox LOST 4th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30, isolation-injected) |
| 90 | PWS_MECHPROB30 | Stuck in 4th gear | cooldown_unblock | player gearbox STUCK in 4th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30) |
| 84 | PWS_MECHPROB24 | Lost 5th gear | cooldown_unblock | player gearbox LOST 5th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30, isolation-injected) |
| 91 | PWS_MECHPROB31 | Stuck in 5th gear | cooldown_unblock | player gearbox STUCK in 5th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30) |
| 85 | PWS_MECHPROB25 | Lost 6th gear | cooldown_unblock | player gearbox LOST 6th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30, isolation-injected) |
| 92 | PWS_MECHPROB32 | Stuck in 6th gear | cooldown_unblock | player gearbox STUCK in 6th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30) |
| 86 | PWS_MECHPROB26 | Lost 7th gear | cooldown_unblock | player gearbox LOST 7th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30, isolation-injected) |
| 93 | PWS_MECHPROB33 | Stuck in 7th gear | cooldown_unblock | player gearbox STUCK in 7th gear: unblock FLT_MAX cooldown to 15 s then say_eid | dev | yes (doc56 Verified gearbox lines: heard 2026-07-30) |
| 3 | PWS_QUAL07 | Pole sitter AI beats own time | dll_emit | MF V3 line MF_OILP01 "Oil pressure dropping.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 8 | PWS_QUAL06 | AI driver moved up position | dll_emit | MF V3 line MF_OILP02 "Oil pressure low, short shift.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 9 | PWS_2001_04 | AI driver fails to improve qualifying position | dll_emit | MF V3 line MF_OILP03 "Low oil pressure, keep the revs down.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 10 | PWS_QUAL23 | Pole sitter player beats own time | dll_emit | MF V3 line MF_OILLEAK01 "You're leaking oil.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 16 | PWS_2001_05 | Player is on Pole Position | dll_emit | MF V3 line MF_OILLEAK02 "Oil leak, take it easy.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 23 | PWS_2001_17 | AI driver starting 1st run | dll_emit | MF V3 line MF_OILT01 "Oil temperature high, back off.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 24 | PWS_2001_18 | AI driver starting 2nd run | dll_emit | MF V3 line MF_BEAR01 "Bearings are going, box now.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 25 | PWS_2001_19 | AI driver starting 3rd run | dll_emit | MF V3 line MF_WATER01 "Water temperature rising.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 26 | PWS_2001_20 | AI driver starting 4th run | dll_emit | MF V3 line MF_WATER02 "Water temp high, lift and coast.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 27 | PWS_2001_22 | Rain expected - make run ASAP | dll_emit | MF V3 line MF_COOLANT01 "You're losing coolant.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 28 | PWS_2001_24 | Rain expected to ease - wait | dll_emit | MF V3 line MF_COOLANT02 "Coolant low, box this lap.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 29 | PWS_2001_25 | Rain expected to continue - make run ASAP | dll_emit | MF V3 line MF_RAD01 "Debris in the radiator.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 30 | PWS_2001_14 | AI driver ahead on Flying Lap | dll_emit | MF V3 line MF_RAD02 "Radiator blocked, find clean air.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 31 | PWS_2001_15 | AI driver ahead on In Lap | dll_emit | MF V3 line MF_RAD03 "Radiator's clear now.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 32 | PWS_2001_13 | AI driver ahead on Out Lap | dll_emit | MF V3 line MF_GASKET01 "Head gasket's gone.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 33 | PWS_2001_08 | AI driver is on Pole Position | dll_emit | MF V3 line MF_VALVE01 "Losing valve pressure.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 36 | RE_OD_EXIT_RED | Other driver under investigation - exiting red | dll_emit | MF V3 line MF_VALVE02 "Valves are floating, short shift.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 37 | RE_OD_LAP_DISAL | Other driver lap disallowed - exited under red | dll_emit | MF V3 line MF_OVERREV01 "Over-rev, watch the downshifts.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 38 | RE_PD_EXIT_RED | Under investigation - exiting under red | dll_emit | MF V3 line MF_OVERREV02 "That over-rev hurt the engine.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 39 | RE_PD_LAP_DISAL | Fastest lap disallowed - exited under red | dll_emit | MF V3 line MF_EXH01 "Exhaust is cracked.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 40 | PWS_2001_33 | X position or better secures WDC | dll_emit | MF V3 line MF_EXH02 "Exhaust problem, keep pushing.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 46 | PWS_MISC54 | Box! Pit now | dll_emit | MF V3 line MF_BLOWBY01 "Engine's losing compression.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 47 | PWS_MISC18 | Stop next lap | dll_emit | MF V3 line MF_ENG01 "We're seeing an engine problem.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 48 | PWS_PIT13 | Stop-go penalty countdown | dll_emit | MF V3 line MF_FUELP01 "Fuel pressure dropping.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 78 | PWS_MECHPROB06 | Front Left puncture | dll_emit | MF V3 line MF_FUELP02 "Fuel pressure low, keep it smooth.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 79 | PWS_MECHPROB07 | Front Right puncture | dll_emit | MF V3 line MF_FUELLEAK01 "You've got a fuel leak.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 58 | PWS_TEAMPIT01 | Teammate pits this lap - stay out | dll_emit | MF V3 line MF_FUELLEAK02 "Fuel leak, box now.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 59 | PWS_TEAMPIT02 | Player can now pit | dll_emit | MF V3 line MF_CRIT01 "Engine critical, box now.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 60 | PWS_2001_96 | Player rejoins track at X position | dll_emit | MF V3 line MF_SAVE01 "Save the engine.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 102 | PWS_MECHPROB08 | Rear Left puncture | dll_emit | MF V3 line MF_DEAD01 "Engine's gone, stop the car.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 103 | PWS_MECHPROB09 | Rear Right puncture | dll_emit | MF V3 line MF_DEAD02 "Engine's failed, park it safely.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 112 | RE_PD_STALLED | Engine stalled - T-car prepared | dll_emit | MF V3 line MF_REPAIR01 "Engine fixed, go.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 113 | RE_PD_STALLED_T | T-car also stalled - session over | dll_emit | MF V3 line MF_CLEAR01 "Problem's cleared, push now.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 71 | PWS_RACEFAST03 | Player sets all-time lap record | dll_emit | MF V3 line MF_HIT01 "Big hit, checking the data.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |
| 72 | PWS_RACEFAST02 | Player sets fastest race lap | dll_emit | MF V3 line MF_HIT02 "Damage after that hit, watch the temps.": donor eid row rewritten at load when PWS_MF01 SDT is installed; fired via commentary queue / radio_req | released | unknown (MF owner live checks pending per project record) |

### red_flag

Version: 1.1.5 public (RELEASE_REGISTRY 2026-09-29); dev src gr44.c, candidate 1.1.8-rc.1

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 131 | PWS_RED01 | Red flag - session stopped | dll_emit | red flag armed (NUMPAD1 / auto monitor): inject_commentary(131) with EVENT_TS cleared | released | unknown (gr44.c:3484 live table read only; no in-game playback record found) |

### full_safety_car

Version: 0.10.66 beta1 public (RELEASE_REGISTRY); dev pace_probe.c (0.10.88 private per CURRENT_STATE)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 132 | PWS_SLOWCAR01 | Slow car ahead | carrier_score | safety-car announcements: score rewritten on carrier eid 132 (SCAR05=131, SCAR01=132, SCAR02=133, SCAR03=134, SCAR04=135) then FN_DRIVER_LOOKUP(132); SLOWCAR01 (own clip) cooldown parked at FLT_MAX so it never plays | released | unknown per clip (CURRENT_STATE.md:25 in-game PASS for build 0.10.66, per-line not recorded) |

### player_damage_commentary

Version: dev (no version; no registry row)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 106 | PWS_HIT03 | Player lost Front Left wheel | cooldown_unblock | player damage event: vanilla emitter fired but cooldown was FLT_MAX; patch sets 15.0 s at attach | dev | yes (player_damage_commentary/FEATURE.md:7 heard in-game 2026-07-27) |
| 107 | PWS_HIT04 | Player lost Front Right wheel | cooldown_unblock | player damage event: vanilla emitter fired but cooldown was FLT_MAX; patch sets 15.0 s at attach | dev | yes (player_damage_commentary/FEATURE.md:7 heard in-game 2026-07-27) |
| 108 | PWS_HIT01 | Player lost front wing | cooldown_unblock | player damage event: vanilla emitter fired but cooldown was FLT_MAX; patch sets 15.0 s at attach | dev | yes (player_damage_commentary/FEATURE.md:7 heard in-game 2026-07-27) |
| 109 | PWS_HIT05 | Player lost Rear Left wheel | cooldown_unblock | player damage event: vanilla emitter fired but cooldown was FLT_MAX; patch sets 15.0 s at attach | dev | yes (player_damage_commentary/FEATURE.md:7 heard in-game 2026-07-27) |
| 110 | PWS_HIT06 | Player lost Rear Right wheel | cooldown_unblock | player damage event: vanilla emitter fired but cooldown was FLT_MAX; patch sets 15.0 s at attach | dev | yes (player_damage_commentary/FEATURE.md:7 heard in-game 2026-07-27) |
| 111 | PWS_HIT02 | Player lost rear wing | cooldown_unblock | player damage event: vanilla emitter fired but cooldown was FLT_MAX; patch sets 15.0 s at attach | dev | yes (player_damage_commentary/FEATURE.md:7 heard in-game 2026-07-27) |

### extended_damage_commentary

Version: 1.0.1-r2 public (RELEASE_REGISTRY 2026-10-02); dev folder v1.0.6 (meta.json 1.0.6)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 106 | PWS_HIT03 | Player lost Front Left wheel | cooldown_unblock | player damage event: same cooldown unblock (15 s) | released | yes (extended_damage_commentary v1.0.6 .c:5 "heard in-game 2026-07-27") |
| 107 | PWS_HIT04 | Player lost Front Right wheel | cooldown_unblock | player damage event: same cooldown unblock (15 s) | released | yes (extended_damage_commentary v1.0.6 .c:5 "heard in-game 2026-07-27") |
| 108 | PWS_HIT01 | Player lost front wing | cooldown_unblock | player damage event: same cooldown unblock (15 s) | released | yes (extended_damage_commentary v1.0.6 .c:5 "heard in-game 2026-07-27") |
| 109 | PWS_HIT05 | Player lost Rear Left wheel | cooldown_unblock | player damage event: same cooldown unblock (15 s) | released | yes (extended_damage_commentary v1.0.6 .c:5 "heard in-game 2026-07-27") |
| 110 | PWS_HIT06 | Player lost Rear Right wheel | cooldown_unblock | player damage event: same cooldown unblock (15 s) | released | yes (extended_damage_commentary v1.0.6 .c:5 "heard in-game 2026-07-27") |
| 111 | PWS_HIT02 | Player lost rear wing | cooldown_unblock | player damage event: same cooldown unblock (15 s) | released | yes (extended_damage_commentary v1.0.6 .c:5 "heard in-game 2026-07-27") |

### player_retire_commentary

Version: candidate (folder release-aligned-1.0.1-dev; PRODUCT_STATUS active-candidate)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 117 | PWS_RETIRE02 | Player retired (generic 1) | dll_emit | retirement: ReasonCode other than 1 or 7 (fallback RETIRE02) | candidate | yes (player_retire_commentary/FEATURE.md:7 RETIRE02 heard in-game 2026-07-28) |
| 118 | PWS_RETIRE01 | Player engine stall | cooldown_unblock | retirement ReasonCode 7: cooldown patched if FLT_MAX | candidate | unknown (FEATURE.md:7 wired, awaiting a race that produces ReasonCode 7) |
| 119 | PWS_RETIRE04 | Player blown engine | cooldown_unblock | retirement ReasonCode 1: cooldown patched if FLT_MAX | candidate | unknown (FEATURE.md:7 wired, awaiting a race that produces ReasonCode 1) |

### player_dq_commentary

Version: dev (no version; no registry row)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 149 | RE_PD_DQ | Disqualified - failed to serve stop/go | cooldown_unblock | player disqualified: cooldown at eid 149 row patched FLT_MAX -> 1.0 s | dev | yes (player_dq_commentary/source.c:5 "fired in-game 2026-07-28"; FEATURE.md:23 FACT ts=313.641) |

### traffic_density_commentary

Version: dev (no version; no registry row)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 11 | PWS_QUAL29 | Some drivers on track | dll_emit | race-pack traffic band "some": eid 11 (on-track filter pittingStatus==0) | dev | yes (clip_assignments H7 note: PWS_QUAL29 live-confirmed 2026-07-28) |
| 12 | PWS_QUAL28 | Very few drivers on track | dll_emit | traffic band "few": eid 12 | dev | yes (traffic_density_commentary/FEATURE.md:25 eid 12 PLAYING, user-confirmed) |
| 15 | PWS_QUAL30 | Most drivers on track | dll_emit | traffic band "most": eid 15 | dev | yes (traffic_density_commentary/FEATURE.md:25 eid 15 PLAYING, user-confirmed) |

### tyre_blister_commentary

Version: dev (no version)

| eid | clip | says | mechanism | condition | release | live |
| --- | --- | --- | --- | --- | --- | --- |
| 104 | PWS_TYRES02 | Severe blistering warning | native_enabled | tyre tread threshold raised (runtime patch at tolerance floats) so native severe-blister emitter is reached in normal running | candidate | no (source.c:3 "REGISTERED, NOT WITNESSED FIRING"; FEATURE.md:7-16 says SHIPPED, conflict) |
| 105 | PWS_TYRES01 | Light blistering warning | native_enabled | tyre tread threshold raised so native light-blister emitter is reached in normal running | candidate | no (source.c:3 "REGISTERED, NOT WITNESSED FIRING"; FEATURE.md:7-16 says SHIPPED, conflict) |

## progress.json says WIP/SILENT but a mod plays it

Compared with `data/clip_assignments.json`. Each row is a mod that already plays the eid while the progress record still says WIP or SILENT.

| eid | progress clip | progress status (batch) | mod | mechanism | release |
| --- | --- | --- | --- | --- | --- |
| 53 | PWS_PIT08 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 54 | PWS_2001_80 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 56 | PWS_2001_79 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 61 | PWS_2001_76 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 63 | PWS_2001_72 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 65 | PWS_2001_73 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 68 | PWS_2001_74 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 70 | PWS_2001_75 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 66 | PWS_2001_77 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 117 | PWS_RETIRE02 | WIP (H3_mechanical) | pitstop_modding | dll_emit | released |
| 183 | PWS_MISC30 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 184 | PWS_2001_131 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 185 | PWS_2001_132 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 186 | PWS_2001_133 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 187 | PWS_2001_134 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 188 | PWS_2001_135 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 189 | PWS_2001_136 | WIP (H1_pit) | pitstop_modding | dll_emit | released |
| 49 | PWS_PIT01 | WIP (H1_pit) | mechanical_failures | dll_emit | released |
| 130 | PWS_MECHANWARN0 | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 119 | PWS_RETIRE04 | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 75 | PWS_MECHPROB16 | WIP (H3_mechanical) | mechanical_failures | dll_emit | dev |
| 76 | PWS_MECHPROB14 | WIP (H3_mechanical) | mechanical_failures | dll_emit | dev |
| 80 | PWS_MECHPROB20 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 87 | PWS_MECHPROB27 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 81 | PWS_MECHPROB21 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 82 | PWS_MECHPROB22 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 83 | PWS_MECHPROB23 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 90 | PWS_MECHPROB30 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 91 | PWS_MECHPROB31 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 85 | PWS_MECHPROB25 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 92 | PWS_MECHPROB32 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 86 | PWS_MECHPROB26 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 93 | PWS_MECHPROB33 | WIP (H3_mechanical) | mechanical_failures | cooldown_unblock | dev |
| 23 | PWS_2001_17 | WIP (H7_session_flags) | mechanical_failures | dll_emit | released |
| 24 | PWS_2001_18 | WIP (H7_session_flags) | mechanical_failures | dll_emit | released |
| 25 | PWS_2001_19 | WIP (H7_session_flags) | mechanical_failures | dll_emit | released |
| 26 | PWS_2001_20 | WIP (H7_session_flags) | mechanical_failures | dll_emit | released |
| 27 | PWS_2001_22 | SILENT (H6_weather_tyres) | mechanical_failures | dll_emit | released |
| 28 | PWS_2001_24 | SILENT (H6_weather_tyres) | mechanical_failures | dll_emit | released |
| 29 | PWS_2001_25 | SILENT (H6_weather_tyres) | mechanical_failures | dll_emit | released |
| 30 | PWS_2001_14 | WIP (H7_session_flags) | mechanical_failures | dll_emit | released |
| 31 | PWS_2001_15 | WIP (H7_session_flags) | mechanical_failures | dll_emit | released |
| 32 | PWS_2001_13 | WIP (H7_session_flags) | mechanical_failures | dll_emit | released |
| 36 | RE_OD_EXIT_RED | WIP (H2_penalties) | mechanical_failures | dll_emit | released |
| 37 | RE_OD_LAP_DISAL | WIP (H2_penalties) | mechanical_failures | dll_emit | released |
| 38 | RE_PD_EXIT_RED | WIP (H2_penalties) | mechanical_failures | dll_emit | released |
| 39 | RE_PD_LAP_DISAL | WIP (H2_penalties) | mechanical_failures | dll_emit | released |
| 40 | PWS_2001_33 | WIP (H5_race_standings) | mechanical_failures | dll_emit | released |
| 46 | PWS_MISC54 | WIP (H1_pit) | mechanical_failures | dll_emit | released |
| 47 | PWS_MISC18 | WIP (H1_pit) | mechanical_failures | dll_emit | released |
| 48 | PWS_PIT13 | WIP (H1_pit) | mechanical_failures | dll_emit | released |
| 78 | PWS_MECHPROB06 | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 79 | PWS_MECHPROB07 | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 58 | PWS_TEAMPIT01 | WIP (H1_pit) | mechanical_failures | dll_emit | released |
| 59 | PWS_TEAMPIT02 | WIP (H1_pit) | mechanical_failures | dll_emit | released |
| 60 | PWS_2001_96 | WIP (H1_pit) | mechanical_failures | dll_emit | released |
| 102 | PWS_MECHPROB08 | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 103 | PWS_MECHPROB09 | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 112 | RE_PD_STALLED | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 113 | RE_PD_STALLED_T | WIP (H3_mechanical) | mechanical_failures | dll_emit | released |
| 71 | PWS_RACEFAST03 | WIP (H5_race_standings) | mechanical_failures | dll_emit | released |
| 132 | PWS_SLOWCAR01 | WIP (H5_race_standings) | full_safety_car | carrier_score | released |
| 117 | PWS_RETIRE02 | WIP (H3_mechanical) | player_retire_commentary | dll_emit | candidate |
| 118 | PWS_RETIRE01 | WIP (H3_mechanical) | player_retire_commentary | cooldown_unblock | candidate |
| 119 | PWS_RETIRE04 | WIP (H3_mechanical) | player_retire_commentary | cooldown_unblock | candidate |
| 149 | RE_PD_DQ | WIP (H2_penalties) | player_dq_commentary | cooldown_unblock | dev |

Notes: eid 132 (PWS_SLOWCAR01) is WIP in progress and full_safety_car uses it as a carrier, but the mod blocks its own clip so it never plays. eid 149 is WIP in doc56 but the player_dq_commentary source header says fired in-game. Eids 11, 12, 15 are DONE in progress and are not flagged as WIP.

## Prior-art contradictions and conflicts

- **MF donor claim vs doc56.** mf_voice.h:4-5 says donor eids have "no native emitter, none used by any first-party mod". Doc56 marks several donors SHIPPED or DONE (see the CONFLICT note on those MF rows, for example eids 3, 8, 9, 10, 16, 33, 72). MF repoints those native lines in the installed build.
- **eid 46.** f1c_ida_decompiled/tools/sdk/f1c_pit_voice.h (allow-list around lines 19-27) allows eid 46 ("Box! Pit now", PWS_MISC54) while mechanical_failures uses eid 46 as donor BLOWBY01 (mf_voice.h). Both cannot own eid 46 in the same install.
- **Duplicate emitters.** eid 51/45/43: pitstop_modding and pit_exit_commentary. eid 106-111: player_damage_commentary and extended_damage_commentary (same patch). eid 117: pitstop_modding and player_retire_commentary. eid 119: mechanical_failures and player_retire_commentary.
- **tyre_blister status.** FEATURE.md says SHIPPED for PWS_TYRES01/02; source.c:3 says REGISTERED, NOT WITNESSED FIRING. Census uses candidate.
- **eid 149 status.** doc56 says WIP; player_dq_commentary/source.c:5 says fired in-game 2026-07-28. Recorded as the source claim, not re-verified here.
- **AI subject.** pit_commentary.c:433-436 passes subject -1 to FN_DRIVER_LOOKUP for AI lines 183-189. The brief rule says AI lines need veh+0x489C. OPEN: check the lookup argument before ERE copies this pattern.
- **Version drift.** Red Flag: CURRENT_STATE.md says 1.1.3 public; RELEASE_REGISTRY says 1.1.5. Safety Car: RELEASE_REGISTRY says 0.10.66 beta1; PRODUCT_STATUS says 0.10.91. Extended Damage: registry 1.0.1-r2; folder meta 1.0.6; RELEASE_CANDIDATE.md says 1.0.2.
- **Score vs eid.** Safety Car scores 131-135 are scores on carrier eid 132, not eids. Doc56 eid 131 (PWS_RED01) is a different row.

## dev-only and abandoned emitters

| where | eids | status | note |
| --- | --- | --- | --- |
| mechanical_failures dll/gear_failure.c (gearbox G8) | 80-93 | dev | JSON dev rows; not in releases/v1.0.7/src |
| mechanical_failures dll/suspension_failure.c, suspension_failure_v5.c | 75 | dev | JSON dev row |
| mechanical_failures dll/electronics_failure.c | 76 | dev | JSON dev row |
| pit_exit_commentary, traffic_density_commentary, player_dq_commentary, player_damage_commentary | 43/45/51, 11/12/15, 149, 106-111 | dev | JSON dev rows |
| tyre_blister_commentary | 104, 105 | candidate | JSON rows |
| f1c_ida_decompiled/mods/red_flag/gr29.c | 131 | prototype (superseded by red_flag/src/gr44.c) | inject_commentary gr29.c:127-139 |
| f1c_ida_decompiled/mods/grid_reorder/gr18.c | 131 (0x83) | prototype fork "adds PWS_RED01" | gr18.c:4, 78-90, 315 |
| f1c_ida_decompiled/mods/safety_car/sc1.c, sc2.c, safety_car.py | 132 carrier (score rewrite) | prototype | sc1.c:120-135, safety_car.py:166-175 |
| f1c_ida_decompiled/mods/extended_vanilla_features (archived, read-only) | pit_exit, retire, traffic copies | archived | superseded by development/mods copies |
| f1c_ida_decompiled/mods/ai_pit_retire, ai_wing_retire, engine_blowup, no_tab_pause | none found | prototype | census negative: no eid emitter in source |
| development/mods/synthetic_pitstop | unknown | dev | only dll and ini present, no source: UNKNOWN |
| development/mods/native_safety_car (dll/native_sc.c) | none found | dev | census negative |
| development/mods/eighth_gear | none found | dev | census negative |
| development/mods/coop_mode dll/coop_mode.cpp:3117 | observer only | released 1.0.3 | logs commentary eid, does not emit (DERIVED) |
| f1c_pit_voice.h allow-list | 44, 46, 50, 57, 62, 64, 67, 69, 237 | SDK helper, not a mod | allowed; no first-party mod plays them (census negative) |
| pitstop_modding pseudo-eids 9001-9005, 9100, 9101 | not table eids | state flags | not passed to the EVENT_TABLE fire path; excluded |

## Limits and UNKNOWN

- No live verification. live_verified cites prior records only.
- MF donor rows depend on the installed SDT carrying PWS_MF01 appended clips (mf_voice.h:31). Without it the remap does not apply (DERIVED).
- MF 1.0.7 public source was checked for eid matches only (releases/v1.0.7/src/mf_voice.h has the donor table); no full diff against dll/.
- Lightloader v2.0.0-hub-dev.1 tree grep for eid/EVENT_TABLE returned no hits (search completed).
- Pitstop AI subject handling (eids 183-189) is OPEN.

## Restoration

Two files written: data/modded_lines.json and docs/research/MODDED_LINES_CENSUS.md. No other file edited. findings.jsonl, governance and production files were read only.


## Coordinator review 2026-10-18 (Claude) — MF donor eids

Checked in `development/mods/mechanical_failures/releases/v1.0.7/src/engine_failure.c`:
- FACT (code read): since 2026-10-07 MF never writes the event table, score lookup `0x5BEBB0` or the SDT
  in release builds (`voice_install_transaction` / `voice_sdt_installed` are `__attribute__((unused))`;
  comment at line 1552). Its 35 lines play as loose WAVs through a detour of `FontRenderingPrep 0x411840`
  (`voice_native_install`, ~line 3954): MF queues its donor eid as a token and, while that line is pending,
  the detour returns MF's WAV path for the queued event.
- So the census rows saying MF "repoints" vanilla eids 3/8/9/10/16/33/72 overstate it: those lines are not
  changed in normal play.
- REAL RISK (DERIVED): while an MF line is pending, any other queued event with the same donor eid plays
  MF's WAV. Donor eids that ERE also wants: 47, 48, 58, 59, 60 (also Pit Stop Failures), 78, 79, 102, 103,
  112, 113, 71, 72, and others in `mf_voice.h` MFV_LINES.
- BUILD RULE for ERE: before queueing an eid, skip or delay if MF has a pending line on that token; and
  propose to MF that its tokens move to eids no line needs (follow-up, MF owner change).
