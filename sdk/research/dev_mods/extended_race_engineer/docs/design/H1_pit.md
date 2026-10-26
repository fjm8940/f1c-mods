# H1_pit design batch (pit procedure, pit crew outcomes, AI pit problems)

Machine record: `data/design/H1_pit.json` (schema ere.design.v1, 34 rows). Research only: no game work, no code.
Owner decisions still open are listed at the end.

## Summary

- 34 clips. 28 are already played by Pit Stop Failures (`development/mods/pitstop_modding/live/pit_live_voice.h`, `live/live_client.cpp`, `dll/pit_commentary.c`). Technique `owned_by_other_mod`.
- 6 clips have no owner in any mod and no native emitter in the static index: PIT01, PIT13, MISC18, TEAMPIT01, TEAMPIT02, AIPIT11. Technique `game_state_emitter`.
- No clip is orphaned or FLT_MAX-blocked: `full_event_dump.json` gives cooldown 1.0 and blocked=false for every PWS eid in this batch (live cooldown table not re-read).
- Every PWS eid here has no literal emitter and no dynamic candidate in `emitter_index.json`, so the native game never plays these PWS lines. The 'silent' status comes from missing emitters, not gates.

## Table (clip, says, technique, trigger, blocker)

| Clip | Says | Technique | Trigger (one line) | Blocker / note |
|---|---|---|---|---|
| PWS_PIT12 | Limiter Off - clear of pit lane | owned (pitstop) | pittingStatus 4->0 | none; FACT isolated |
| PWS_PIT10 | Go (pit procedure) | owned (pitstop) | pittingStatus 3->4 | none; FACT isolated |
| PWS_MISC54 | Box! Pit now | owned (pitstop) | lane (status 2), car position veh+0x34E0/E4/E8 within 10 m of own PitPos | pitstop uses a service-position distance; owner expects box |
| PWS_MISC18 | Stop next lap | game_state_emitter | once per lap: fuel for one lap or scheduled stop next lap | fuel margin, player schedule readable? |
| PWS_PIT13 | Stop-go penalty countdown | game_state_emitter | pending stop-go and pittingStatus 2->3 | stop-go record locator UNKNOWN |
| PWS_PIT01 | Pit on this lap | game_state_emitter | pittingStatus 0->1 before S/F on this lap | S/F lap-distance threshold UNKNOWN |
| PWS_PIT03 | Watch for pit lane entrance | owned (pitstop) | call active, status <=2, near XPITIN | isolation of 50 undocumented |
| PWS_PIT06 | Limiter On | owned (pitstop) | pittingStatus 0/1->2 | none; isolation-tested |
| PWS_PIT08 | Brake (pit procedure) | owned (pitstop) | pittingStatus 2->3 | none |
| PWS_2001_80 | Engine stalled - will get refired | owned (pitstop) | player STALL_DELAY fault | TV 337 native (TV only) |
| PWS_2001_79 | Fuel hose issue - slow refuel | owned (pitstop) | player FUEL_RIG fault | none |
| PWS_2001_78 | Refuel successful | owned (pitstop) | fuel fault and refuel complete | none |
| PWS_TEAMPIT01 | Teammate pits this lap - stay out | game_state_emitter | teammate (same team index) status 1 and player stop within one lap | team pairing rule; pit-order meaning |
| PWS_TEAMPIT02 | Player can now pit | game_state_emitter | player pit order active and teammate status 2/3 | same as TEAMPIT01; priority rule |
| PWS_2001_96 | Player rejoins track at X position | owned (pitstop) | race exit with position (native voice_rejoin) | bypasses SDK allowlist |
| PWS_2001_76 | Wheel gun failure | owned (pitstop) | player WHEELGUN fault | pitstop owns timer delay |
| PWS_2001_68 | Front Left tire change successful | owned (pitstop) | FL recovered after fault only | not on normal stops by design |
| PWS_2001_72 | Slow Front Left tire change | owned (pitstop) | FL SLOW_CORNER fault | none |
| PWS_2001_69 | Front Right tire change successful | owned (pitstop) | FR recovered after fault | fault-only |
| PWS_2001_73 | Slow Front Right tire change | owned (pitstop) | FR SLOW_CORNER fault | none |
| PWS_2001_77 | Wheel is jammed | owned (pitstop) | CROSS_THREAD outcome | none |
| PWS_2001_70 | Rear Left tire change successful | owned (pitstop) | RL recovered after fault | fault-only |
| PWS_2001_74 | Slow Rear Left tire change | owned (pitstop) | RL SLOW_CORNER fault | none |
| PWS_2001_71 | Rear Right tire change successful | owned (pitstop) | RR recovered after fault | fault-only |
| PWS_2001_75 | Slow Rear Right tire change | owned (pitstop) | RR SLOW_CORNER fault | none |
| PWS_MISC30 | AI has pit stop issue | owned (pitstop) | AI generic pit fault | Safety Car AI filter |
| PWS_2001_131 | AI driver engine stalled in pits | owned (pitstop) | AI STALL_DELAY | TV 516 contradiction (below) |
| PWS_2001_132 | AI driver jammed fuel hose | owned (pitstop) | AI FUEL_RIG | none |
| PWS_2001_133 | AI driver stuck Front Left wheel | owned (pitstop) | AI CROSS_THREAD wheel 0 | none |
| PWS_2001_134 | AI driver stuck Front Right wheel | owned (pitstop) | AI CROSS_THREAD wheel 1 | none |
| PWS_2001_135 | AI driver stuck Rear Left wheel | owned (pitstop) | AI CROSS_THREAD wheel 2 | none |
| PWS_2001_136 | AI driver stuck Rear Right wheel | owned (pitstop) | AI CROSS_THREAD wheel 3 | triage text mismatch |
| PWS_AIPIT13 | AI rejoins track at X position | owned (pitstop) | AI race exit with position | owner status SILENT conflicts |
| PWS_AIPIT11 | AI pits to serve stop-go penalty | game_state_emitter | AI pending stop-go and status 1->2 | AI stop-go state UNKNOWN; TV 510 conflict |

## Coordinator rule applied (skip and native-but-unheard)

- No row uses `skip`. Every clip has a design (owned, or a game_state_emitter).
- Three PWS eids have a native TV twin: 54 (TV 337), 184 (TV 516), 195 (TV 510). Diagnoses are stored per row as `native_diagnosis` in the JSON.
  - PWS 54 and 184: pitstop already queues them. The native TV lines are not the PWS path. No ERE change.
  - PWS 195: no native PWS emitter. The 510 literal is weather. ERE queues 195 itself.
- Position and heading (coordinator-confirmed FACT): car position `veh+0x34E0/E4/E8`; heading rotation matrix `veh+0x34BC..0x34E0` (cos at 0x34BC and 0x34DC, sin at 0x34C4, -sin at 0x34D4). MISC54 now uses position against PitPos instead of a lane distance.

## Shared state and hooks this batch needs

- Pit state: `veh+0x3450` pittingStatus (player and AI). Enum 0 track, 1 in-lap, 2 lane, 3 box, 4 exit (findings topic 'pittingStatus full enum', CONFIRMED).
- Lap: `veh+0x403C` lapNumber (CONFIRMED). Lap distance `veh+0x4044` (FACT per brief, not re-read in code).
- Subject: `veh+0x489C` driver id (required for eid >= 183). Team: `veh+0x48A0` (constructor index, not unique; CONFIRMED).
- Fuel: `veh+0x3984` FuelLevel, `veh+0x3990` fuel per lap (DERIVED, PIT_STOP_FIRING_ORDER.md:208-209).
- Schedule: pit object `veh+0x32CC`, count `+0x342C`, laps `+0x3430+8i` (DERIVED, AI-side evidence).
- Stop-go: no record locator. StopGoPenaltyHandler 0x4BBD60 sets STOP_GO with 10 s (SPEC_EVF-003). Needed for PIT13 and AIPIT11.
- Session: `0x7C56A4` SessionType, `0x7D0308` RaceActiveFlag (FACT).
- Queue: enqueue through `f1c_pit_voice.h` (SDK allowlist). Eids 60 and 190 bypass it in pitstop.
- Hook point for ERE: a per-car observer after the pit state update each tick (EMITTER_SPECS.md pit_procedure).

## Prior-art contradictions found

1. **pittingStatus enum.** EVF-004A and `red_flag` treat 2 as 'in box'. Pitstop live traces and findings (CONFIRMED, 4+ cycles) say 2 = lane, 3 = box. The findings say the red_flag comment at `development/mods/red_flag/redflag.c:585` is stale. Use the traced enum.
2. **PitExit and eid 516/184.** SPEC_EVF-008_E says PitExit emits eid 184 (AI engine stalled). `emitter_index.json` dynamic candidates for PitExit are 397, 516-519, 523, 526-527, 530-533. 184 is not one of them. Code at `functions/PitExit_0041c0f0.c:38` sets 516 for a position-1 exit, which does not match the 'engine stalled' label in doc56.
3. **eid 510.** `functions/PitRaceStatusMessageDispatcher_0041be60.c:34` emits 510 as a race-status line. doc56 labels 510 as PWS_AIPIT11's TV twin.
4. **EVF-004A 'no fuel offset'.** EVF-004A says FuelLevel is not found. PIT_STOP_FIRING_ORDER.md gives `veh+0x3984` (DERIVED).
5. **EVF-004A 'schedule not readable'.** EVF-004A says the player schedule is not in the vehicle struct. The later finding gives `veh+0x3430` (DERIVED, AI-side).
6. **EVF-008 F 'UNKNOWN emitter' for eid 54.** Pitstop plays eid 54 for the player stall; the native emitter for its TV twin 337 is `DisplayMessage0x151`. Not a conflict for ERE, but note it for future audits.
7. **AIPIT13 status.** Owner status is SILENT, but `live_client.cpp:534` queues eid 190.

## Open questions (owner decision or live test)

- MISC54: lane (pitstop) or box (owner)? Live check.
- PIT01 and MISC18: S/F threshold, one-lap fuel margin, player schedule readability.
- PIT13 and AIPIT11: where the stop-go record and its countdown live.
- TEAMPIT01/02: meaning of 'pit order' for team calls; team pairing; priority among TEAMPIT01, TEAMPIT02, PIT01.
- 2001_68, 69, 70, 71: owner asked whether they can play on normal stops. Pitstop plays them only after a fault.
- 2001_131 and 516: isolation-test which TV eid is heard before any ERE change.
- 2001_136: triage text describes a rejoin with tyre choice, not a stuck wheel. Check the owner's note.
- Safety Car filter (`development/mods/full_safety_car/dll/pace_probe.c:3186-3210`): suppresses AIPIT eids 178-182 and 196 per driver. Confirm it covers 183-190 and 195 as well.
- Pitstop source of record: `live/` is the path used by `live_client.cpp`; `src/` and `dll/` are older duplicates. Which build is shipped was not checked by H1_pit.

## Not verified by this batch

- No live reads, no isolation injection, no build. All 'live' items above are pending.
- Tiers: CONFIRMED and FACT come from findings or pitstop docs. DERIVED is static. GUESS is for stop-go and team rows.
