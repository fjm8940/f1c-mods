# Observer Table — T3 clip groups for the config-driven observer engine

<!-- 97 T3 clips from data/progress.json, grouped by shared state observer -->

## Grouping (all 97 T3 rows → 10 observers)

| # | Observer group | Clips | Shared state |
|---|---------------|-------|-------------|
| 1 | **PENALTY_STEWARD** | 24 (**only 7 buildable** — 13 AI clips proven dead 2026-07-28, 4 need DANGEROUS_DRIVING) | Player-only tracker at veh slot0+0x4034; penalty type = handler arg2; count = veh+0x4070 |
| 2 | **PIT_PROCEDURE** | 16 | Player pittingStatus + pit box state + fuel level + tyre change flags |
| 3 | **MECHANICAL_DAMAGE** | 9 | Vehicle failure status/category + gear fail flag + tyre puncture state |
| 4 | **SESSION_PHASE** | 8 | lapNumber + TotalRaceLaps + SessionType + session timer |
| 5 | **POSITION_OVERTAKE** | 8 | Player position change + car-to-car gap + blue flag state + lapped-car detection |
| 6 | **AI_PIT_EXIT_TYRE** | 7 | AI pittingStatus + nextChosenTires + tyre compound + pit penalty status |
| 7 | **WEATHER_CHANGE** | 4 | OnLineWetness + OffLineWetness + cloud/rain state |
| 8 | **INCIDENT_STATE** | 5 | Off-track byte + spin detection + yellow flag sector state + green flag transition |
| 9 | **RACE_PACE** | 3 | AI LivePaceFactor + AbilityBaselineScalar + position delta |
| 10 | **UNCLASSIFIED** | 13 | Misc clips that each need unique state (retire/stall/orphan/qualifying-runs/SCAR) |

### Clip inventory per group

**1. PENALTY_STEWARD (24 clips)**

*Player penalties (8):*

| Clip | SDT | EID | Penalty type |
|------|----:|-----|-------------|
| PWS_PENALTY08 | 148 | 139 | 25 sec time penalty for 3+ blue flag ignores |
| PWS_MISC47 | 153 | 144 | Track cut penalty (investigation outcome) |
| PWS_MISC04 | 155 | 146 | Overtaking under yellow |
| PWS_MISC03 | 156 | 147 | Speeding in pitlane |
| RE_PD_INV_DD | 159 | 150 | Dangerous driving investigation |
| RE_PD_SG_SPEED | 162 | 153 | Pit speeding (note: EXISTS, double check) |
| PWS_PENALTY04 | 163 | 154 | Stop/go for blocking under blue flag |
| PWS_PENALTY03 | 165 | 156 | Stop/go for overtaking under yellow flag |

*AI penalties (16):*

| Clip | SDT | EID | Penalty type |
|------|----:|-----|-------------|
| PWS_AIPENALTY14 | 256 | 240 | AI 25 sec time penalty — track cut |
| PWS_AIPENALTY07 | 257 | 241 | AI 25 sec time penalty — overtaking under yellow |
| PWS_AIPENALTY05 | 258 | 242 | AI 25 sec time penalty — pitlane speeding |
| PWS_AIPENALTY22 | 259 | 243 | AI disqualified — failed to serve stop/go |
| PWS_MISC48 | 261 | 245 | AI cutting track |
| PWS_MISC06 | 262 | 246 | AI jump start |
| PWS_MISC08 | 263 | 247 | AI overtaking under yellow |
| PWS_MISC07 | 264 | 248 | AI speeding in pits |
| RE_OD_25_DD | 265 | 249 | AI 25 sec penalty — dangerous driving |
| RE_OD_INV_DD | 266 | 250 | AI investigation — dangerous driving |
| RE_OD_SG_DD | 267 | 251 | AI stop/go — dangerous driving |
| PWS_AIPENALTY04 | 268 | 252 | AI stop/go — blocking under blue |
| PWS_AIPENALTY13 | 269 | 253 | AI stop/go — cutting track |
| PWS_AIPENALTY02 | 270 | 254 | AI stop/go — jump start |
| PWS_AIPENALTY03 | 271 | 255 | AI stop/go — overtaking under yellow |
| PWS_AIPENALTY01 | 272 | 256 | AI stop/go — speeding in pits |

**2. PIT_PROCEDURE (16 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_PIT12 | 42 | 43 | Player exits the pit straight |
| PWS_PIT13 | 47 | 48 | 10 second stop and go — player drives into pit box |
| PWS_PIT01 | 48 | 49 | Pitting this lap — before crossing S/F line |
| PWS_PIT03 | 49 | 50 | Near pit lane entrance |
| PWS_PIT08 | 52 | 53 | Player enters pit box, pit timer starts |
| PWS_2001_78 | 56 | 57 | Refueling ends (fuel level stable) |
| PWS_TEAMPIT02 | 58 | 59 | Team mate has pitted |
| PWS_2001_96 | 59 | 60 | Rejoined in position X |
| PWS_2001_69 | 63 | 64 | Front right tyre ok |
| PWS_2001_73 | 64 | 65 | Tyre change ok |
| PWS_2001_70 | 66 | 67 | Rear left ok |
| PWS_2001_74 | 67 | 68 | Tyre change ok |
| PWS_2001_71 | 68 | 69 | Rear left ok |
| PWS_2001_75 | 69 | 70 | Tyre change ok |
| PWS_MISC30 | 195 | 183 | Problem in the pits (pit stop delay) |
| PWS_2001_132 | 197 | 185 | Fuel hose jammed |

**3. MECHANICAL_DAMAGE (9 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_MECHPROB06 | 77 | 78 | FL puncture |
| PWS_MECHPROB07 | 78 | 79 | Puncture (different corner) |
| PWS_MECHPROB08 | 101 | 102 | Puncture (different corner) |
| PWS_MECHPROB09 | 102 | 103 | Puncture (different corner) |
| RE_PD_FL_BLIST | 94 | 95 | Front left blister |
| PWS_MECHPROB20 | 79 | 80 | Player losing a gear |
| PWS_AIMECHPROB0 | 230 | 201,202,212-217,447-448,463-467,471 | AI gear loss (12+ eid variants: lost 1st, lost 2nd, stuck in 1st, stuck in 2nd...) |
| RE_OD_LOST_7 | 222 | 209 | Lost seventh gear |
| RE_OD_STUCK_7 | 224 | 211 | Stuck in seventh gear (in T1 per WORK_CATEGORIES) |

**4. SESSION_PHASE (8 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_2001_165 | 287 | 270 | Three quarter distance (75%) |
| PWS_2001_164 | 288 | 271 | Half distance (50%) |
| PWS_2001_163 | 292 | 274 | One quarter distance (25%) |
| PWS_2001_159 | 293 | 275 | Session over (time reached 0:00) |
| PWS_RACEFAST03 | 70 | 71 | All-time race lap record |
| PWS_2001_102 | 72 | 73 | Personal best lap |
| PWS_2001_33 | 39 | 40 | Played to the player (qualifying/practice lap review) |
| PWS_2001_13 | 30 | 32 | Practice/qualifying lap review |

**5. POSITION_OVERTAKE (8 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_AILAPPIN05 | 125 | 121 | Player loses a position |
| PWS_AILAPPIN01 | 126 | 122 | Blue flag driver behind player |
| PWS_AIBLUE02 | 273 | 257 | Player approaching a lapped car |
| PWS_AIBLUE01 | 274 | 258 | Same |
| PWS_AIBLUE03 | 275 | 259 | Passed lapped car |
| PWS_MISC46 | 209 | 197 | Faster car overtaking a lapped car |
| PWS_OVERTAKE02 | 123 | — | Overtake (orphaned — no eid) |
| PWS_OVERTAKE03 | 124 | — | Overtake (orphaned — no eid) |

**6. AI_PIT_EXIT_TYRE (7 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_2001_136 | 201 | 189 | AI driver leaves pits, position said out loud |
| RE_OD_INTER | 203 | 191 | AI leaves pits, on INTERMEDIATE tyres |
| RE_OD_MONSOON | 204 | 192 | AI leaves pits, on MONSOON tyres |
| RE_OD_SLICKS | 205 | 193 | AI leaves pits, on SLICKS (wet conditions) |
| RE_OD_WET | 206 | 194 | AI leaves pits, on WET tyres |
| PWS_AIPIT11 | 207 | 195 | AI has stop/go penalty and is serving it |
| PWS_2001_141 | 212 | 199 | Stub — probably not to be done yet |

**7. WEATHER_CHANGE (4 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_2001_218 | 300 | 282 | Clouding over, possible rain |
| PWS_2001_220 | 301 | 283 | Rain starting, short shower |
| PWS_2001_219 | 302 | — | Rain starting, light drizzle (orphaned) |
| PWS_2001_222 | 299 | 281 | Rain easing off |

**8. INCIDENT_STATE (5 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_GREEN01 | 130 | 126 | Yellow flag zone ends OR yellow flag goes off |
| PWS_OFFROAD02 | 117 | — | Off track (orphaned) |
| PWS_AIINCID02 | 242 | — | AI off somewhere sector 1/2/3 (orphaned) |
| PWS_AIINCID04 | 246 | 232 | Spun off in sector 1/2/3 |
| PWS_AIINCID05 | 247 | — | Has spun (orphaned) |

**9. RACE_PACE (3 clips)**

| Clip | SDT | EID | What it says |
|------|----:|-----|-------------|
| PWS_MISC28 | 283 | 266 | Is holding position |
| PWS_MISC27 | 284 | 267 | Same as above |
| PWS_MISC25 | 286 | 269 | Is pushing hard |

**10. UNCLASSIFIED (13 clips)**

| Clip | SDT | EID | What it says |
|------|-----|-----|-------------|
| PWS_2001_17 | 21 | 23 | Practice/qualifying line |
| PWS_2001_18 | 22 | 24 | Practice/qualifying line |
| PWS_2001_19 | 23 | 25 | Practice/qualifying line |
| PWS_2001_20 | 24 | 26 | Practice/qualifying line |
| PWS_2001_14 | 28 | 30 | Practice/qualifying line |
| PWS_2001_15 | 29 | 31 | Practice/qualifying line |
| RE_OD_EXIT_RED | 35 | 36 | AI exits under red |
| RE_OD_LAP_DISAL | 36 | 37 | Lap disallowed |
| PWS_RETIRE03 | 119 | — | Retirement line (orphaned) |
| RE_PD_STALLED_T | 112 | 113 | Terminal stall → DNF |
| PWS_SCAR05 | 137 | 428 | Safety car line (UNDER WORK VIA ANOTHER MOD) |
| PWS_2001_110 | 177 | — | Check if loaded by game (orphaned) |
| PWS_2001_149 | 249 | 234 | Same as 244 but reverse |

### Clips that refuse to group

These 13 unclassifieds each need unique state or are orphans:
- **2001_17-20, 2001_14, 2001_15** (6 clips): Qualifying/practice lap performance review. Notes just say "X play" — human didn't elaborate. May share with QUALIFYING_RUNS spec or be entirely separate mechanism.
- **RE_OD_EXIT_RED, RE_OD_LAP_DISAL** (2 clips): AI-specific exits under red flag. "AI likely doesnt exit under red so might be redundant."
- **PWS_RETIRE03** (1 clip): Orphaned — no eid. Even if it had one, the retire emitter is proven unreachable; would need DLL like RETIRE02.
- **RE_PD_STALLED_T** (1 clip): Terminal stall → DNF. eid 113 is FLT_MAX-blocked. No known detection mechanism in vehicle struct.
- **PWS_SCAR05** (1 clip): Safety car. "UNDER WORK VIA ANOTHER MOD."
- **PWS_2001_110** (1 clip): Orphaned + "check if loaded by the game already."
- **PWS_2001_149** (1 clip): Position holding — reverse of sdt 244. Could join POSITION_OVERTAKE but note says "maybe these should play 50/50", implying randomness, which no other position clip has.

---

## Observer 1: PENALTY_STEWARD (24 clips — largest group)

### Shared state

Observer watches penalty handler `sub_4BCCC0`'s output state via the
per-frame infraction tracker at `GlobalInstance_5EA560` (0x5EA560, CONFIRMED)
and per-vehicle penalty counters. The existing `SPEC-penalty_steward` already
identifies the core detection mechanisms — this table extends it to the 24
unemitted commentary lines.

### Key offsets

| Offset / name | Tier | Source | Persistence | Notes |
|--------------|------|--------|------------|-------|
| `GlobalInstance_5EA560` (0x5EA560) | CONFIRMED | name_index.json | persistent ptr | The infraction tracker object. 20 readers, called by MainGameTickUpdate. |
| `tracker+0xD4` (blue flag byte) | DERIVED | sub_4BCCC0.c:324 `this[212] = 1` | LATCHED | Set when another car is lapping player (RacePhase==7). Value=1 means blue flag active. |
| `tracker+0xDC` (blue flag timer) | DERIVED | sub_4BCCC0.c:374 `this[55]` float seconds | LATCHED | >=30.0 triggers IgnoredBlueFlag penalty. |
| `tracker+0xB4` (off-track byte) | DERIVED | sub_4BCCC0.c:214 `this[180] = 1` | LATCHED | Set when lateral distance > 2.5m from centerline. |
| `veh+0x4070` noOfPenaltiesToProcess | CONFIRMED | name_index.json | static=0 in all observed samples | int32. Read by PerDriverRaceCommentary. No known writer in corpus — may only be set briefly during penalty dispatch. |
| `dword_7CEF84` PenaltiesEnabled | DERIVED | name_index.json, globals_usage | LATCHED | 5 readers (sub_4BCCC0 among them). Gates ALL penalty enforcement. |
| `dword_7CEF3C` PenaltyConfigFlag | DERIVED | name_index.json | LATCHED | Config writeback flag. |
| `veh+0x4064` FinishState | FACT | name_index.json, live poke | LATCHED | 1=finished, 2=post-session, 3=DQ'd. Gates penalty processing for classified cars. |
| `veh+0x32C8` slot-live | FACT | name_index.json, live poke | LATCHED | -1=empty slot. Used to iterate AI drivers. |
| `SessionType` 0x7C56A4 | FACT | live poke | LATCHED | 5=qual, 7=race. Penalties are race-only. |
| `RaceActiveFlag` 0x7D0308 | FACT | live poke | LATCHED | 3=on track. Gates commentary emission. |
| `lapNumber` veh+0x403C | CONFIRMED | name_index.json | LATCHED | Race restart detect: lap going backwards. |

### Per-clip table — PENALTY_STEWARD

---

#### PLAYER PENALTIES

**PWS_PENALTY08** (SDT 148, eid 139)
| Field | Value |
|-------|-------|
| observer_group | PENALTY_STEWARD |
| watch_offsets | `tracker+0xD4` (blue flag byte, DERIVED), `tracker+0xDC` (blue flag timer, DERIVED), `veh+0x4064` (FinishState, FACT) |
| arm_condition | `tracker+0xD4 == 1` AND `tracker+0xDC >= 30.0` (third blue flag ignored) |
| fire_condition | `veh+0x4064 == 0` (driver still classified) AND 3+ blue flag triggers counted since race start |
| cancel_condition | `veh+0x4064 != 0` (driver retired/DQ'd before penalty applied) OR RaceActiveFlag != 3 |
| latch | once-per-race |
| reset_on | lapNumber decreasing (race restart) |
| cooldown | 1.0 (from full_event_dump.json eid 139) |
| priority | 70 |
| uncertainty | The "3 blue flag triggers" count must be tracked by the observer; sub_4BCCC0's internal trigger count is a local variable, not a persistent field. Must either hook sub_4BCCC0 or maintain a parallel counter watching tracker+0xD4 transitions from 0→1. The `ignoredBlueFlag` penalty assignment path in sub_4BCCC0 writes to an unknown member — need to find which offset stores the penalty type. |

**PWS_MISC47** (SDT 153, eid 144), **PWS_MISC04** (SDT 155, eid 146), **PWS_MISC03** (SDT 156, eid 147)
| Field | Value |
|-------|-------|
| observer_group | PENALTY_STEWARD |
| watch_offsets | `tracker+0xB4` (off-track byte, DERIVED for MISC47 cut track), `veh+0x4070` (noOfPenaltiesToProcess, CONFIRMED), `veh+0x4068` (ReasonCode, FACT for DQ), sub_4BCCC0 penalty-type output (UNKNOWN offset) |
| arm_condition | Penalty assigned to player by sub_4BCCC0: MISC47=cut track (lateral distance > 1.75), MISC04=overtaking under yellow, MISC03=pitlane speeding (> 7.5 m/s in pits) |
| fire_condition | Penalty type matches the clip AND investigation timer expired (see MISC05 in T5 for investigation framework) OR penalty applied immediately |
| cancel_condition | lapNumber decreasing (restart) OR player leaves session |
| latch | once-per-race per penalty type |
| reset_on | race restart (lapNumber decreasing) |
| cooldown | 1.0 (all three eids: 144, 146, 147) |
| priority | 70 |
| uncertainty | CRITICAL — `sub_4BCCC0`'s penalty type output is NOT located at any known persistent offset. The string-tag-to-eid mapping in `sub_41A720` maps `CutTrack→151`, `OvertakingUnderYellow→?` (not tracked), `SpeedingInPitl→153` — but these eids (151, 153) are DIFFERENT from our target eids (144, 146, 147). Our clips are the race-engineer commentary BETWEEN the infraction and the penalty, not the penalty announcement itself. The "under investigation" framework does NOT exist in the shipped code — it would need to be built. Until the penalty-type output location is found, these 3 clips are **unbuildable** for the observer engine. Mark as: needs sub_4BCCC0 hook or penalty-type discovery. |

**RE_PD_INV_DD** (SDT 159, eid 150)
| Field | Value |
|-------|-------|
| observer_group | PENALTY_STEWARD |
| watch_offsets | `veh+0x4070` (noOfPenaltiesToProcess, CONFIRMED), `dword_7CEF84` (PenaltiesEnabled, DERIVED), penalty-type field (UNKNOWN) |
| arm_condition | Player under investigation for dangerous driving — penalty-type = "dangerous driving" |
| fire_condition | Investigation started AND investigation timer < time_to_decision |
| cancel_condition | Penalty decided (timer expired) OR race restart |
| latch | once-per-race |
| reset_on | lapNumber decreasing |
| cooldown | 1.0 (eid 150) |
| priority | 70 |
| uncertainty | "Dangerous driving" has NO known detection mechanism in the decompiled corpus. No string tag maps to it, no sub_4BCCC0 branch handles it. The investigation framework does not exist. This clip is **unbuildable** until a dangerous-driving decision rule is specified. |

**RE_PD_SG_SPEED** (SDT 162, eid 153)
| Field | Value |
|-------|-------|
| observer_group | PENALTY_STEWARD |
| watch_offsets | sub_4BCCC0 pit-speeding detection output (v10 > 7.5, UNKNOWN persistent offset) |
| arm_condition | Speed in pitlane > 7.5 m/s (sub_4BCCC0.c:111) |
| fire_condition | Penalty assigned to player for pit speeding |
| cancel_condition | None (fires on penalty assignment) |
| latch | once-per-race |
| reset_on | race restart |
| cooldown | 1.0 (eid 153) |
| priority | 70 |
| uncertainty | Human note says "EXISTS, double check" — this clip MAY already play in vanilla. Verify before building. Also: even if sub_4BCCC0 detects the infraction and assigns a penalty, the commentary output for the "stop/go" variant (as opposed to a time penalty) may be emitted by a different path. `sub_41A720` maps `SpeedingInPitl→153` (AI) / `256` (player) — eid 153 in our table IS the same eid sub_41A720 emits for AI, which suggests our clip might already be handled for AI but not player. |

**PWS_PENALTY04** (SDT 163, eid 154), **PWS_PENALTY03** (SDT 165, eid 156)
| Field | Value |
|-------|-------|
| observer_group | PENALTY_STEWARD |
| watch_offsets | penalty-type field (UNKNOWN), `dword_7CEF84` (PenaltiesEnabled, DERIVED), `veh+0x4064` (FinishState, FACT) |
| arm_condition | Player assigned stop/go penalty: PENALTY04=blocking under blue, PENALTY03=overtaking under yellow |
| fire_condition | Penalty type matches AND player has NOT yet served the stop/go |
| cancel_condition | Player serves stop/go OR race restart |
| latch | once-per-race per penalty type |
| reset_on | race restart |
| cooldown | 20.0 (eid 154), 1.0 (eid 156) |
| priority | 70 |
| uncertainty | Same as MISC47/04/03: no known persistent penalty-type field. The "blocking under blue" infraction has no detection mechanism in sub_4BCCC0. "Overtaking under yellow" — the game DOES detect overtaking under yellow (RacePhase check), but the decision to penalize and which commentary to emit is unclear. Both clips are **unbuildable** until penalty-type output location is found. |

---

#### AI PENALTIES

All 16 AI penalty clips share one observer structure:

**Shared AI penalty observer**

| Field | Value |
|-------|-------|
| observer_group | PENALTY_STEWARD |
| watch_offsets | `VehiclePtrArray` 0x7C54F0 (FACT), `veh+0x32C8` slot-live (FACT), `veh+0x4620` driver name (FACT), player `veh+0x4068` ReasonCode (FACT for DQ check), AI penalty assignment output (UNKNOWN) |
| arm_condition | Any AI driver (slot-live != -1, not the player) receives a penalty from the game's penalty system |
| fire_condition | Penalty type matches clip AND penalty is applied (not just investigation) |
| cancel_condition | AI retires/DQ'd before line fires OR race restart |
| latch | repeatable+cooldown (different AI drivers can each trigger) |
| reset_on | race restart |

**AI penalty type → clip mapping:**

| Penalty type | Sub-type | EID | Clip (SDT) | Detection mechanism |
|-------------|----------|-----|-----------|-------------------|
| 25 sec | Track cut | 240 | AIPENALTY14 (256) | sub_4BCCC0 cut-track + time penalty |
| 25 sec | Overtake yellow | 241 | AIPENALTY07 (257) | sub_4BCCC0 yellow overtake + time penalty |
| 25 sec | Pit speeding | 242 | AIPENALTY05 (258) | sub_4BCCC0 pit speed + time penalty |
| 25 sec | Dangerous driving | 249 | RE_OD_25_DD (265) | UNKNOWN |
| 25 sec | Blocking blue | — | (no clip with 25 sec blue for AI? AIPENALTY04 is stop/go) | — |
| Stop/go | Speeding pits | 256 | AIPENALTY01 (272) | sub_4BCCC0 + AI stop/go |
| Stop/go | Jump start | 254 | AIPENALTY02 (270) | Jump-start detection (separate mechanism) |
| Stop/go | Overtake yellow | 255 | AIPENALTY03 (271) | sub_4BCCC0 + AI stop/go |
| Stop/go | Blocking blue | 252 | AIPENALTY04 (268) | UNKNOWN |
| Stop/go | Track cut | 253 | AIPENALTY13 (269) | sub_4BCCC0 + AI stop/go |
| Stop/go | Dangerous driving | 251 | RE_OD_SG_DD (267) | UNKNOWN |
| Investigation | Dangerous driving | 250 | RE_OD_INV_DD (266) | UNKNOWN |
| Investigation | Infraction | — | MISC48 (261, cut track), MISC06 (262, jump start), MISC08 (263, overtake yellow), MISC07 (264, pit speed) | sub_4BCCC0 detection + investigation flag |
| DQ | Failed stop/go | 243 | AIPENALTY22 (259) | DQ detection (FinishState=3 on AI) |

**Uncertainty (applies to entire AI penalty group):**

1. **Does the game actually assign penalties to AI drivers?** The triage notes consistently say "not sure if the game enforces AI penalties." The human flagged this as OPEN. `sub_4BCCC0` handles PLAYER infractions — it may or may not run AI infractions through the same path. Until confirmed: **all 16 AI penalty clips are potentially unbuildable** because the triggering event may never occur.

2. **sub_41A720 already emits 4 penalty types for AI** (SpeedingInPitl→153, CutTrack→151, JumpedStart→155, IgnoredBlueFla→154) — but our target eids (240-256) are DIFFERENT. These are additional lines that sub_41A720 does NOT emit. The game may have never been designed to emit them.

3. **Dangerous driving has no detection mechanism anywhere in the codebase.** Clips 249, 250, 251 (RE_OD_25_DD, RE_OD_INV_DD, RE_OD_SG_DD) are blocked on defining "dangerous driving." Same for "blocking under blue" (252).

4. **"Investigation" lines** (MISC48, MISC06, MISC08, MISC07, RE_OD_INV_DD) need an investigation timer framework that does not exist in the shipped code. The human proposed (in T5 note for MISC05) a 30-60 second random delay between investigation and penalty — building this is a new subsystem, not just a tracker.

5. **Penalty-type output location** — the field that records WHAT penalty was assigned and to whom — is UNKNOWN. Without it, the observer cannot distinguish a stop/go from a 25s penalty, nor a cut-track penalty from a pit-speeding penalty. Must either hook sub_4BCCC0 to read its internal decision, or find the persistent struct where penalty records are stored.

6. **`veh+0x4068` ReasonCode == 9 (engine blowup)** is the only penalty-related offset with a confirmed trigger path. The broader penalty system's storage format is not documented.

---

### Unbuildable penalty rows (8 of 24)

These clips need a field or mechanism that nobody has identified:

| Clip | Blocker | What's missing |
|------|---------|---------------|
| PWS_MISC47 | Penalty-type output offset | Can't distinguish cut-track penalty from other penalties |
| PWS_MISC04 | Penalty-type output offset | Can't distinguish overtake-yellow from other penalties |
| PWS_MISC03 | Penalty-type output offset | Can't distinguish pit-speeding penalty from others |
| RE_PD_INV_DD | No "dangerous driving" detection | No code path, no string tag, no sub_4BCCC0 branch |
| PWS_PENALTY04 | No "blocking under blue" detection | No code path for this infraction type |
| PWS_PENALTY03 | Penalty-type output offset | Same root cause as MISC04 |
| RE_OD_25_DD | No "dangerous driving" detection | Same as RE_PD_INV_DD |
| RE_OD_INV_DD | No "dangerous driving" detection | Same |
| RE_OD_SG_DD | No "dangerous driving" detection | Same |

The remaining 16 penalty clips (5 player + 11 AI) are buildable only AFTER the penalty-type offset is located. Until then, all share the same blocker.

**Searches performed for penalty-type output:**
- `tools/ask.py offset 0x4070` → `noOfPenaltiesToProcess`: int32, static=0 in all observed samples, no known writer. This field is either unused in-race or written too briefly to catch.
- `tools/ask.py findings penalty steward` → 0 results matching penalty-type storage.
- `functions/sub_4BCCC0_004bccc0.c`: penalty type is assigned to a local variable, not written to a persistent offset.
- `name_index.json`: no entry for penalty-type, penalty-record, or infraction-type.
- `config_schema.json`: no penalty-type token found.
- `persistence_map.jsonl`: no penalty-type offset classified.
