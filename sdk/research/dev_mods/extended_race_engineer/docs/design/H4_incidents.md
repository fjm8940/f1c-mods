# H4 incidents - design batch (research only), revision 2

Date: 2026-10-18. Machine-readable rows: `data\design\H4_incidents.json` (schema ere.design.v1, 21 rows).
No live game work, no edits to EVF, corpus, other mods or findings.jsonl. All eids come from
`f1c_ida_decompiled\mods\extended_vanilla_features\data\full_event_dump.json` (events) and
`emitter_index.json`. Tiers: FACT > CONFIRMED > DERIVED > GUESS; UNKNOWN where no source exists.

Revision 2 (coordinator review): spin lines now use the heading matrix; AIINCID10/11 redesigned
(native emitter found behind a packet gate); MISC26/27/28 designed from observed state. No row is
"skip" any more. The skip rule is reserved for lines that cannot be real.

## Summary table

| Clip | Says | Technique | Trigger (one line) | Blocker |
|---|---|---|---|---|
| PWS_HIT03 | Player lost FL wheel | owned_by_other_mod | sub_41AFD0 a2=0 -> eid 106 | none (EDC v1.0.6, FACT live) |
| PWS_HIT04 | Player lost FR wheel | owned_by_other_mod | a2=1 -> eid 107 | none (EDC) |
| PWS_HIT01 | Player lost front wing | owned_by_other_mod | a2=4 -> eid 108 | none (EDC) |
| PWS_HIT05 | Player lost RL wheel | owned_by_other_mod | a2=2 -> eid 109 | none (EDC) |
| PWS_HIT06 | Player lost RR wheel | owned_by_other_mod | a2=3 -> eid 110 | none (EDC) |
| PWS_HIT02 | Player lost rear wing | owned_by_other_mod | a2=5 -> eid 111 | none (EDC) |
| PWS_AIINCID10 | AI drivers collide (1) | hook_native | keep native sub_41AD80; AI-AI only: 2 s per-car guard, a4 > 1250, random pick, queue insert | a4 units / AI impact level unknown; measure first (no cooldown fix: cd 1.0) |
| PWS_AIINCID11 | AI drivers collide (2) | hook_native | same native call, alternate pick (flt_8AB274 <= 0) | same |
| PWS_AIINCID06 | Car off track just ahead | game_state_emitter | AI off-surface, just ahead of player, < 10 s | no emitter; per-track drivable tags |
| PWS_AIINCID03 | AI went off track | game_state_emitter | same detector, any AI in range | radius rule is an owner decision |
| PWS_AIINCID08 | Cars all over in sector X | game_state_emitter | >= 3 crashed/stopped AI in one sector | no emitter; red_flag gate needed |
| PWS_AIINCID07 | Car spun off just ahead | game_state_emitter | slip (heading vs travel) + yaw rate + speed drop, AI ahead of player < 10 s | thresholds UNKNOWN (calibrate) |
| PWS_AIINCID04 | AI spun off sector X | game_state_emitter | same spin detector, sector | thresholds UNKNOWN (calibrate) |
| PWS_AIINCID01 | AI stalls on the grid | game_state_emitter | native stall roll (Failure Rate 1-2) | grid RacePhase unknown; ReasonCode 7/3/8 meaning |
| PWS_OD_HOLDING | AI holds up another AI | game_state_emitter | adjacent AI pair gap < 0.5 s for 5 s | gap from distance/speed (DERIVED); shared latch |
| PWS_2001_149 | AI held up by another AI | game_state_emitter | same pair, car behind is subject | same as OD_HOLDING |
| PWS_MISC29 | AI reduces pace (2) | game_state_emitter | veh+0x453C drops below baseline | margin unknown; AAD conflict |
| PWS_MISC28 | AI maintains position (2) | game_state_emitter | position held 3 laps with a car within 1.5 s behind | proposed split, owner to confirm |
| PWS_MISC27 | AI maintains position (1) | game_state_emitter | position held 3 laps in clean air, gap stable | proposed split, owner to confirm |
| PWS_MISC26 | AI reduces pace (1) | game_state_emitter | clean-lap average speed 3 % below own previous 3 laps, 2 laps | thresholds DERIVED |
| PWS_MISC25 | AI increases pace | game_state_emitter | veh+0x453C walks up toward ceiling | margin unknown; AAD conflict |

Counts (revision 2): owned_by_other_mod 6, hook_native 2, game_state_emitter 13, skip 0.

## Findings that matter

1. **HIT01-06 are owned by extended_damage_commentary v1.0.6** (`development\mods\extended_damage_commentary\v1.0.6\extended_damage_commentary.c:1-16`). The game emitter is sub_41AFD0 (`functions\sub_41AFD0_0041afd0.c`). Blocker was event cooldown FLT_MAX; EDC writes 15.0 s. Findings "PROVEN LIVE" covers all six. ERE should not patch these eids again.
2. **AIINCID10/11: re-diagnosed from sub_41AD80 (r3, DERIVED; the r2 packet-gate claim is WITHDRAWN).** `NetworkPacketDataReader_0054fa90` is a contact-buffer slot reader (this[120], stride 116), not network code, and sub_41AD80 also emits player-hit eids 114/115 that were heard in vanilla, so the path runs in single player. The AI-AI branch runs only when neither car is the player. Gates in order: (1) race flags (`sub_41AD80_0041ad80.c:~22-24`); (2) per-car 2 s guard for both cars, written before the emit, so an earlier contact blocks later lines; (3) impact a4 > 1250.0 (units UNKNOWN; player contacts pass the same gate). The random pick `flt_8AB274[(u8)dword_8AAE70++] > 0` chooses 225, otherwise 226; it cannot silence the event, but its cursor is shared with `AudioEventRingBufferProcessing_004feaf0.c:64`. Queue: `InsertDriverTableEntry_00418560.c` gates on score (228 for 225, passes settings 1-3), keeps a 20-slot list sorted by play time, evicts the lowest score when full, and returns 0 silently when the new time is not earlier than any slot. Cooldown: the table gives cd 1.0, not FLT_MAX, so it is not a block; SPEC_EVF-008_D's 30.0 is not supported. No cooldown or priority fix is justified. Technique hook_native: keep the native call and measure. Log a4, the 2 s guard and the insert return value on AI-AI contacts and on one player contact as a control. Then choose the fix: a4 below 1250 means the gate is real and the line stays silent by design, the guard or eviction means a queue fix.
3. **No native emitter exists for eids 224-234 and 265-269** (emitter_index literal and dynamic lists are empty for them). They are reachable on the PWS channel through their own score (dump: `via_pws_lookup`, cd 1.0, not blocked), so direct insertion is possible.
4. **The AI off-track byte veh+0x40E8 is player-only** (findings off-track-byte-40e8-player-only-2026-10-02). AI off-track uses per-wheel material tags, which vary by track.
5. **Heading is a matrix (FACT).** Heading = atan2(veh+0x34C4, veh+0x34BC). The matrix is veh+0x34BC..0x34DC. veh+0x34F0 is a derived mirror (findings f1c-car-heading-is-a-rotation-matrix-not-the-yaw-field; EVF CLAUDE rows 2026-08-03h, 2026-08-03c). World position is veh+0x34E0/E4/E8 (FACT).
6. **Real velocity is not stored as a triple.** veh+0x58/0x5C/0x60 is a mirror (findings f1c-veh-velocity-58-is-a-mirror, DISPROVEN). A full-struct scan found no stable triple. Velocity is therefore derived from position deltas (DERIVED). Speed scalar veh+0x4440 is FACT.
7. **AI stalls are native.** sub_424080 rolls StartStalls (veh+0x44BC) in race Failure Rate 1-2 (ai-start-stall-roll-2026-10-01, CONFIRMED). The line can only play when the game's own stall happened, which matches the owner rule.
8. **Native pace walk.** veh+0x453C moves +-0.01 toward baseline veh+0x44F4 or the ceiling every 4 s, traffic-driven via veh+0x4508 (aad-pace-walk-4s-no-rubberband-2026-10-15; corrects the AIAbility skeleton comment).
9. **Yellow zone table** (Steward 0x8ACEE0, +0x10C count, +0x110 stride 8, state 2 = yellow) is rebuilt every frame by sub_4D1730. It is the game's slow-car marker: a prefilter for the spin and count lines. Yellow RAISE emits 134/136/138 and CLEAR emits 133/135/137 (findings f1c-flag-event-dispatcher-is-41b3c0, CONFIRMED).
10. **red_flag gr44.c** has a crash-terminal predicate (gr44.c:8011), a 3-car / 300 m cluster (gr44.c:1286-1299), and a settled-race gate. Its auto-detect is disabled (gr44.c:1230) after false red flags on grid restarts. Reuse the predicate, not the auto-trigger.

## Spin detector (AIINCID07 / 04), revision 2

- Heading angle = atan2(veh+0x34C4, veh+0x34BC) (FACT). Yaw rate = d(yaw)/dt per tick, wrapped (DERIVED).
- Travel direction = atan2(dZ, dX) of veh+0x34E0/E8 deltas over 0.5 s (DERIVED).
- Slip = wrap(heading - travel). Spin = slip >= SPIN_SLIP held for SPIN_T1, AND |yaw rate| >= SPIN_YAW, AND speed (veh+0x4440) drop >= SPIN_DROP within SPIN_T2.
- Gates: race (dword_7C56A4 == 7), pittingStatus veh+0x3450 < 2, engine on (rpm >= 100, so a stall is not a spin), not in the pit polygons.
- Calibration needed: all five thresholds are UNKNOWN. The heading-to-travel sign and offset must first be checked on a car driving straight, because the matrix axis convention is not fixed by evidence.

## Shared state and hooks this batch needs

- Vehicle reads: race position veh+0x4060, lap veh+0x403C, lap distance veh+0x4044, speed veh+0x4440, pittingStatus veh+0x3450 (FACT), subject driver id veh+0x489C (FACT), race session dword_7C56A4 (FACT; 7 = race).
- Heading veh+0x34BC..0x34DC matrix (FACT); position veh+0x34E0/E4/E8 (FACT).
- AI pace: veh+0x453C (OBSERVED), baseline veh+0x44F4 (FACT), ceiling add veh+0x44D8 (CONFIRMED), traffic veh+0x4508 (OBSERVED), throttle cap veh+0x457C (CONFIRMED), finish state veh+0x4064 (DERIVED).
- Stall: veh+0x44BC (CONFIRMED), ReasonCode veh+0x4068 (values 7/3/8 only), restart timer veh+0x4170 (DERIVED).
- Zone table 0x8ACEE0 (CONFIRMED); AIW keys lap_length, sector_1_length, sector_2_length (CONFIRMED).
- Native AI-AI call: dword_5EB978 controller, sub_41AD80 (CONFIRMED).
- One shared battle-pair latch (OD_HOLDING, 2001_149, MISC28 precedence); one shared spin/off detector (AIINCID07, 04, 06, 03).
- Timers: do not depend on dbl_8AFEA0 (OPEN divergence). Use per-tick accumulation with clamping.
- Insertion: InsertDriverTableEntry on controller flt_5EA618 with the subject driver id.

## Contradictions and corrections (not resolved unless stated)

- **PitState vs pittingStatus (corrected).** veh+0x4040 (PitState, used by red_flag OFF_PITSTATE at gr44.c:406) and veh+0x3450 (pittingStatus) are two different fields. Not a contradiction. H4 uses 0x3450 for pittingStatus and does not use 0x4040.
- Cooldown of eids 225/226: SPEC_EVF-008_D says 30.0; full_event_dump says 1.0. Unresolved.
- EVF SPEC_EVF-008_D calls sub_41AD80 "live collision physics". The r2 claim that the call sits behind a network gate is withdrawn (finding 2); the path runs in single player. SPEC_EVF-008_D's cd 30.0 remains unsupported by the table (cd 1.0).
- EDC comment cites eids 220-223 as AI counterparts; sub_41AFD0 only references 221-223 (minor).
- EVF 002E gap candidate veh+0x4098 is labelled "line time" in the AAD row. The native gap field is still unidentified; the H4 design derives gap from distance/speed.
- Root CLAUDE.md says veh+0x443C is recalculated per frame; finding 0x443C-persists says it is not (not used here).
- eids 72/262 fire from the race branch of AIOvertakeDefendStateChange (functions\AIOvertakeDefendStateChange_00419690.c:43-70) but are named PWS_RACEFAST02 and PWS_AIFASTLAP01 in the dump. Not reused.

## Open questions (owner or live)

1. Units of v65 / a4 for the 1250 threshold, and the AI-AI impact distribution (AIINCID10/11). Does the play queue drop entries after their expiry (play time + cd + 0.2)? Unverified.
2. Spin thresholds (SPIN_SLIP, SPIN_YAW, SPIN_T1, SPIN_DROP, SPIN_T2) and the heading axis convention.
3. Meaning of ReasonCode 7/3/8 and the grid RacePhase value (AIINCID01).
4. Per-track drivable material set (AIINCID06/03).
5. Gap check of the distance-derived gap against the game's timing screen (OD_HOLDING).
6. MISC27/28 split: owner to confirm "clean air" vs "pressured".
7. Owner wording for MISC26 (performance trend when held by traffic).
8. Whether adaptive_ai_difficulty (AAD) is active. It writes veh+0x44F4 and would make pace lines describe the mod.

## Verification performed

- JSON rewritten and parsed with `python -I`: 21 rows; technique counts owned_by_other_mod 6, hook_native 2, game_state_emitter 13.
- Heading matrix and velocity claims checked against findings.jsonl ids named above, not against CLAUDE rows alone.
- Eid, emitter and cooldown values read from full_event_dump.json and emitter_index.json.
- Call-site gate read from functions\CollisionDetectionSystem_004ea970.c and functions\NetworkPacketDataReader_0054fa90.c.
- Not verified live: all thresholds, the packet-buffer state in single player, and the impact units.

## Changed files

- Rewritten: `data\design\H4_incidents.json`, `docs\design\H4_incidents.md`. Nothing else changed.
