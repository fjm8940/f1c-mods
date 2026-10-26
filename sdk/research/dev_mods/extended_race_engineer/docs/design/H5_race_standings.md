# H5 race standings and battles: design summary

Batch: H5_race_standings. Machine data: `data/design/H5_race_standings.json`.
Tiers: FACT > CONFIRMED > DERIVED > GUESS. Nothing here is live-proven. `eid_isolation_tested` is false for every row.
Owner goal (coordinator note): every line gets a design or a fix path. `skip` is used only where no situation can occur.

## Per-clip table

| Clip | Says | Technique | Trigger (one line) | Blocker |
|---|---|---|---|---|
| PWS_OVERTAKE01 (120) | Player overtakes AI (1) | hook_native | Native player-pass branch in sub_419120; gate logger finds the first false gate | Reverted: never heard live |
| PWS_AIBLUE03 (259) | Player passes AI driver | hook_native | Same native branch, 25% sub-branch | Reverted; 10 s cooldown |
| PWS_AILAPPIN05 (121) | AI driver passes player | game_state_emitter | AI position crosses from behind to ahead of player, not P1, not pit | No native writer for 121 |
| PWS_AILAPPIN01 (122) | AI driver approaches player | game_state_emitter | AI behind player, gap shrinking under threshold | Thresholds unknown; 421 is name-variant |
| PWS_AIBLUE01 (258) | Player approaches AI (blue flag) | game_state_emitter | Blue-flag byte this+212 goes 0 to 1 in race (sub_4BCCC0:324) | One line per edge (owner pick vs 257) |
| PWS_AIBLUE02 (257) | AI under blue flag, be ready | game_state_emitter | Same flag edge, subject this[54] | Mutually exclusive with 258 per edge |
| PWS_SLOWCAR01 (132) | Slow car ahead | owned_by_other_mod | Slow-car predicate drafted (LivePaceFactor) | Full Safety Car owns eid 132; owner rule |
| PWS_RACEFAST03 (71/343) | Player sets all-time lap record | game_state_emitter | Race lap below ERE-held track record table | Game record store not located |
| PWS_2001_102 (73/344) | Player sets personal best lap | game_state_emitter | New session best via LapCompletion this+0x68 (not first lap) | Tracker-to-vehicle link unknown; eid 73 Albert Park name clash |
| PWS_AIFASTLAP03 (263) | AI sets all-time fastest lap | game_state_emitter | AI lap below the same record table | Same record question |
| PWS_2001_33 (40/331) | X position secures WDC | game_state_emitter | Points model says player's WDC is secure | Points table not read; eid 40 A-1 Ring clash, 331 name variant |
| PWS_2001_141 (199/513) | Team positioned to secure WCC | game_state_emitter | Team points model says WCC secure | Points model; eid 199 is 'Mark W' variant |
| PWS_2001_138 (200/506) | AI positioned to secure WDC | game_state_emitter | AI WDC secure on same model | Points model; eid 200 'Mark W' variant |
| PWS_MISC46 (197/507) | AI driver laps another AI | game_state_emitter | AI A lap count exceeds B by 1 and A passes B at lap boundary | Lap field identity; eid 197 'Marc G' variant |

Totals: hook_native 2, game_state_emitter 11, owned_by_other_mod 1, skip 0.

## Conflict: PWS_SLOWCAR01 (eid 132) and the Safety Car mod

- Full Safety Car (`development\mods\full_safety_car\dll\`) uses eid 132 as its carrier for every safety-car line.
- It parks eid 132's cooldown at FLT_MAX every tick (`pace_probe.c:2478`, re-armed at `:6149`; `pace_probe_standings.c:2538-2560`, re-armed at `:6603`).
- It opens an 8 s window (`CARRIER_INFLIGHT_MS`) for its own lines.
- Its source notes "SLOWCAR01 is not to be played at all" (`pace_probe_standings.c:2605`).
- Newest source: `pace_probe_standings.c` (2026-08-07 fork). `mt_sc.dll` (17:49) has no matching source in `dll\`, so its build is UNKNOWN.
- Rule for ERE: never write or unblock eid 132, never use it as a carrier. The owner must rule on SLOWCAR01 before any design can ship.

## Prior-art contradictions

1. **EVF "NO_KNOWN_WRITER" for eids 199 and 200** (progress.json, PWS_2001_141 and PWS_2001_138). The name lookup `DriverNameIDLookup_00415d90.c:187` and `:407` returns 199 and 200 for 'Mark W'. These eids have a writer.
2. **Name-variant eids**: 71 (David C), 197 (Marc G), 199/200 (Mark W), 331 (Ricardo Z), 421 (Takuma S) are per-driver sentence IDs. Their clip labels may be name-specific.
3. **Track-name eids**: 40 (A-1 Ring, `TrackNameDisplayIDLookup_00417f60.c:64`) and 73 (Albert Park, `:113`) are track sentences on the native path.
4. **Lap time location**: EMITTER_SPECS.md:177 cites lap time at veh+0x60. `LapCompletion_004bc4e0.c:46-56` writes this+0x18 and this+0x68 on a tracker object, not a vehicle. The veh+0x60 claim is unverified.
5. **Dispatcher writers**: 257, 258, 259, 263 (sub_412E50) and 343/344 (sub_412490) have native writers through SoundEventDispatcher. The message types that reach them are not traced. A native line may already fire in live play.

## Shared state and hooks this batch needs

- **Position/overtake**: race position veh+0x4060 (FACT), player slot via 0x7C54E8 (FACT), subject id veh+0x489C (FACT), pit status veh+0x3450 (FACT). Per-tick scan for AI passes (PWS_AILAPPIN05) and a read-only gate logger at sub_419120 entry (OVERTAKE01, AIBLUE03).
- **Blue flag**: this+212 flag byte and this[54] target (DERIVED, sub_4BCCC0:324/327). Blue-flag object is the player slot0 +0x4034 (measurement stands; the inference was retracted in findings.jsonl).
- **Lap events**: LapCompletion_004bc4e0 lap time (a2), this+0x18, this+0x68 best. Needs the tracker-to-vehicle mapping.
- **Record table**: one ERE-held persisted per-track record table, shared by RACEFAST03 and AIFASTLAP03. Seed from the game's record if located.
- **Points model**: season and constructor points, races remaining. Read from ChampionshipPointsAccumulator_0041f220.c and ChampionshipScore_0041eda0.c (not yet read).
- **Lap counter**: veh+0x403C vs 0x4040 identity (MISC46).

## Open questions

1. Which gate is false live in sub_419120 (OVERTAKE01, AIBLUE03)? Gate logger run needed.
2. Do the sub_412E50 / sub_412490 dispatch paths fire 257, 258, 259, 263, 343, 344 live? Trace which message types reach SoundEventDispatcher cases 0xE, 0x13, 0x29, 5, 0xA, 0x17.
3. Owner decision: 257 vs 258 on the same blue-flag edge.
4. Owner ruling on SLOWCAR01 and the carrier question.
5. Where is the game's all-time track record stored? Not in this batch.
6. Points model: read ChampionshipPointsAccumulator and ChampionshipScore for the table and the points field.
7. Tracker-to-vehicle mapping for LapCompletion.
8. Isolation tests for eids 40, 331, 199, 200, 71, 197 before any clip label is trusted.

Scope: static read-only analysis. No live game work, no injection, no edits outside this batch's two outputs.
