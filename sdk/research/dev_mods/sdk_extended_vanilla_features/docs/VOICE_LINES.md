# Voice lines: what is done, what is not, and how each was reached

304 lines total.

| | Count |
|---|---:|
| **Done** - you can hear it because of work in this project | 9 |
| **Not done, game has the code** - it can already say it, the code just never runs | 70 |
| **Not done, game has no code** - nothing in the game ever says it | 108 |
| **Orphaned** - audio file exists, nothing references it | 13 |
| **Unknown** - nobody has checked whether it plays | 104 |

## Per category

Categories are derived by regex on the clip name - `progress.json` has no category
field. Convenience grouping, not a schema. `docs/OBSERVER_TABLE.md` has a better one
for the 97 T3 clips.

| Category | Done by hand | Game has code | Game has no code | Orphaned | Unknown | Total |
|---|---:|---:|---:|---:|---:|---:|
| Season / run plan | 0 | 16 | 32 | 2 | 17 | 67 |
| Other | 0 | 5 | 15 | 9 | 37 | 66 |
| Penalty / steward | 1 | 27 | 15 | 0 | 6 | 49 |
| Mechanical / tyres | 0 | 4 | 21 | 0 | 4 | 29 |
| Pit | 0 | 4 | 8 | 0 | 11 | 23 |
| Misc | 0 | 5 | 13 | 0 | 4 | 22 |
| Qualifying | 0 | 4 | 0 | 0 | 13 | 17 |
| Damage - impact | 6 | 0 | 0 | 0 | 7 | 13 |
| Retirement | 1 | 2 | 0 | 1 | 5 | 9 |
| Flags / safety car | 1 | 2 | 2 | 1 | 0 | 6 |
| Blue flag | 0 | 1 | 2 | 0 | 0 | 3 |
| **TOTAL** | **9** | **70** | **108** | **13** | **104** | **304** |

## Why the 104 are "unknown"

| Reason recorded on the row | Count |
|---|---:|
| blog-confirmed heard **and** reachable live | 57 |
| "not in the blog's confirmed-heard list - unverified, listen by ear" | 37 |
| blog-confirmed heard **but not reachable live** (blocked/orphaned) | 10 |

49 of 104 were set by hand, 55 automatically. The only "vanilla" evidence is one
blog post (j21csblog 2023-01-21); nobody on this project has listened to them.
Per-clip list of all 104: `docs/FULL_INVENTORY.md`.

---

## DONE (12) - how each was reached

### PWS_QUAL28/29/30 (SDT 10/9/13) - traffic density during qualifying

**Reached by: DLL.** `dll/traffic_density_commentary_v5.dll`. Two dead ends first (findings.jsonl): the high eids (302/303/306, same names as this group's table below) hit an unrelated team-bio narration table via `TeamNameDisplayIDLookup_0041b980` - live-confirmed wrong clip both as raw queue-write and as real function call. Fix was the LOW identity-mapped eids from the same `data/full_event_dump.json` (11/12/15) through the same `InsertDriverTableEntry` pipeline - eid 15 live-confirmed to play the real PWS_QUAL30 clip. Second bug found live: the on-track count used `veh+0x32C8` (active/loaded flag, true for all 22 grid cars from session start) instead of `veh+0x3450` (`pittingStatus`, CONFIRMED live: 4 = pit/garage even while moving, 0 = genuinely on track) - fixed by filtering on `pittingStatus==0`. v5 adds a 60s global cooldown across all three bands on top of the existing per-band once-per-session latch.

### 6 damage lines - PWS_HIT01..06 (SDT 105-110)

Front wing, rear wing, all four wheels.

**Reached by: byte patch.** `patches/manifest.json` -> `player-damage-comment`: the cooldown for eids 106-111 was FLT_MAX (never replay). Set to 15.0. 4-byte write, whole fix.

### PWS_RETIRE02 (SDT 118) - Player retired (generic 1)

**Reached by: DLL.** `dll/retire_commentary_v5.dll`. The game HAS an emitter (`sub_41B250`) but it is unreachable for a human driver. The DLL watches for the retirement the game itself performs and injects eid 117 into the queue.

### PWS_RED01 (SDT 135) - Red flag - session stopped

**Reached by: other mod.** Fires through the separate `mods/red_flag` mod, not through work in this folder.

### RE_PD_DQ (SDT 158) - Disqualified - failed to serve stop/go

**Reached by: nothing.** Already fired in vanilla. Proven live 2026-07-28 (controller +0xAF8, eid 149). No code was written - it was mismarked.

---

## NOT DONE - the game already has the code (67)

For every line below, a real function in the game speaks it. It is silent only
because that function is never called with the right arguments during play.
Hooking ONE function unlocks its whole group.

### `TeamNameDisplayIDLookup_0041b980` - 18 lines

QUAL28/29/30 moved to DONE above via a low-eid twin in `full_event_dump.json`
reachable through the same `InsertDriverTableEntry` insertion pipeline (NOT the
high 293-336 eid named in the table below, which hits an unrelated team-bio
table - live-DISPROVEN 2x). All 18 lines below CONFIRMED (2026-07-28, static)
to have the same low-eid twin, same score/sdt_index pairing - the emitter path
is not a dead end for any of them. Not yet live-tested. What's still open per
clip is the trigger CONDITION (rain expected, red-flag investigation, WDC
clinch, pit procedure...) - check `docs/EMITTER_SPECS.md` (2026-07-27 batch)
before re-deriving those.

| Clip | SDT | eid | low-eid twin | What it says |
|---|---:|---|---:|---|
| `PWS_2001_17` | 21 | 314 | 23 | AI driver starting 1st run |
| `PWS_2001_18` | 22 | 315 | 24 | AI driver starting 2nd run |
| `PWS_2001_19` | 23 | 316 | 25 | AI driver starting 3rd run |
| `PWS_2001_20` | 24 | 317 | 26 | AI driver starting 4th run |
| `PWS_2001_22` | 25 | 318 | 27 | Rain expected - make run ASAP |
| `PWS_2001_24` | 26 | 319 | 28 | Rain expected to ease - wait |
| `PWS_2001_25` | 27 | 320 | 29 | Rain expected to continue - make run ASAP |
| `PWS_2001_14` | 28 | 321 | 30 | AI driver ahead on Flying Lap |
| `PWS_2001_15` | 29 | 322 | 31 | AI driver ahead on In Lap |
| `PWS_2001_13` | 30 | 323 | 32 | AI driver ahead on Out Lap |
| `RE_OD_EXIT_RED` | 35 | 327 | 36 | Other driver under investigation - exiting red |
| `RE_OD_LAP_DISAL` | 36 | 328 | 37 | Other driver lap disallowed - exited under red |
| `RE_PD_EXIT_RED` | 37 | 329 | 38 | Under investigation - exiting under red |
| `RE_PD_LAP_DISAL` | 38 | 330 | 39 | Fastest lap disallowed - exited under red |
| `PWS_2001_33` | 39 | 331 | 40 | X position or better secures WDC |
| `PWS_PIT12` | 42 | 334 | 43 | Limiter Off - clear of pit lane |
| `PWS_PIT09` | 43 | 335 | 44 | Gear (pit procedure) |
| `PWS_PIT10` | 44 | 336 | 45 | Go (pit procedure) |

### `SeasonDisplayIDResolver_0041daf0` - 7 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_PENALTY07` | 150 | 490 | Stop/go penalty |
| `PWS_MISC05` | 152 | 494 | Player blocking blue - investigation |
| `PWS_MISC47` | 153 | 499 | Player cutting track - investigation |
| `PWS_MISC04` | 155 | 503 | Player overtaking yellow - investigation |
| `PWS_MISC03` | 156 | 502 | Player speeding pits - investigation |
| `RE_PD_25_DD` | 157 | 478 | 25 sec penalty - dangerous driving |
| `RE_PD_INV_DD` | 159 | 480 | Under investigation - dangerous driving |

### `PitRaceStatusMessageDispatcher_0041be60` - 6 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `RE_OD_MONSOON` | 204 | 511 | Other driver leaves pits on Monsoons |
| `RE_OD_WET` | 206 | 509 | Other driver leaves pits on Wets |
| `PWS_AIPIT11` | 207 | 510 | AI pits to serve stop-go penalty |
| `PWS_MISC46` | 209 | 507 | AI driver laps another AI |
| `PWS_2001_141` | 212 | 513 | Team positioned to secure WCC |
| `PWS_2001_138` | 213 | 506 | AI driver positioned to secure WDC |

### `sub_41A720_0041a720` - 6 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `RE_PD_SG_SPEED` | 162 | 153 | Stop/Go penalty - speeding in pits |
| `PWS_PENALTY04` | 163 | 154 | Blocking blue penalty |
| `PWS_AIPENALTY04` | 268 | 252 | AI blocking blue penalty |
| `PWS_AIPENALTY13` | 269 | 253 | AI penalty (type 13) |
| `PWS_AIPENALTY02` | 270 | 254 | AI speeding in pits penalty |
| `PWS_AIPENALTY01` | 272 | 256 | AI penalty (type 1) |

### `RaceCommentary_0041d630` - 4 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `RE_OD_LOST_7` | 222 | 457 | Other driver lost 7th gear |
| `RE_OD_PUNCTURE` | 223 | 458 | Other driver has a puncture |
| `PWS_AITYRES02` | 231 | 472 | AI severe blistering |
| `PWS_AITYRES01` | 232 | 473 | AI light blistering |

### `PitSelectionStateMachine_0041b3c0` - 4 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_GREEN01` | 130 | 126 | Green flag conditions |
| `PWS_YELLOW04` | 142 | 133 | Sector 1 cleared |
| `PWS_YELLOW05` | 144 | 135 | Sector 2 cleared |
| `PWS_YELLOW06` | 146 | 137 | Sector 3 cleared |

### `AISessionPhaseCounter_0041aa10` - 4 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `RE_PD_FL_BLIST` | 94 | 95 | Front Left light blistering |
| `RE_PD_FR_BLIST` | 96 | 97 | Front Right light blistering |
| `RE_PD_RL_BLIST` | 98 | 99 | Rear Left light blistering |
| `RE_PD_RR_BLIST` | 100 | 101 | Rear Right light blistering |

### `RaceCommentary_0041d630 + sub_41AC50_0041ac50` - 3 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_AIMECHPROB1` | 221 | 208,449,450,451,452,453 |  |
| `RE_OD_STUCK_7` | 224 | 211,462 | Other driver stuck in 7th gear |
| `PWS_AIMECHPROB0` | 230 | 212,213,214,215,216,217,447,448,463,464,465,466,467,471 |  |

### `PitStateSlotActivation_0041ab30` - 3 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `RE_PD_FR_BAD_BL` | 95 | 96 | Front Right severe blistering |
| `RE_PD_RL_BAD_BL` | 97 | 98 | Rear Left severe blistering |
| `RE_PD_RR_BAD_BL` | 99 | 100 | Rear Right severe blistering |

### `sub_41B250_0041b250` - 2 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_RETIRE01` | 120 | 118 | Player engine stall |
| `PWS_RETIRE04` | 121 | 119 | Player blown engine |

### `ConstructorStandingsParser_00420820` - 2 lines

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_SCAR01` | 138 | 423 | Safety car deployed - exiting pits |
| `PWS_SCAR03` | 140 | 424 | Safety car ending - green this lap |

### `PitExit_0041c0f0` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_2001_131` | 196 | 516 | AI driver engine stalled in pits |

### `DisplayMessage0x113_0041a6b0` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_2001_159` | 293 | 275 | Session ends |

### `DisplayMessage0x151_0041c3e0` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_2001_80` | 53 | 337 | Engine stalled - will get refired |

### `AIDriverStateTransition_00419120` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_AIBLUE03` | 275 | 259 | Player passes AI driver |

### `sub_41A940_0041a940` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_AIPENALTY22` | 259 | 243 | AI penalty (type 22) |

### `GdbSeasonSelector_00420cf0` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_PENALTY03` | 165 | 496 | Overtaking yellow penalty |

### `PitExit_0041c0f0 + PositionChangeDetector_00420490` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_QUAL32` | 0 | 293,397 | Team front row lockout |

### `ScoringClassificationDisplay_0041e320` - 1 line

| Clip | SDT | eid | What it says |
|---|---:|---|---|
| `PWS_SLOWCAR01` | 136 | 427 | Slow car ahead |

---

## NOT DONE - the game has no code for these (121)

Nothing in the binary ever speaks these. Restoring one means writing a new
emitter, not just finding a trigger. 108 have an event id but no emitter;
13 are orphaned audio with no event at all.

| Clip | SDT | eids | What it says |
|---|---:|---|---|
| `PWS_MISC54` | 45 | 46 | Box! Pit now |
| `PWS_MISC18` | 46 | 47 | Stop next lap |
| `PWS_PIT13` | 47 | 48,355 | Stop-go penalty countdown |
| `PWS_PIT01` | 48 | 49,363 | Pit on this lap |
| `PWS_PIT03` | 49 | 50,367 | Watch for pit lane entrance |
| `PWS_PIT06` | 50 | 51,356 | Limiter On |
| `PWS_PIT08` | 52 | 53,361 | Brake (pit procedure) |
| `PWS_2001_79` | 55 | 56,365 | Fuel hose issue - slow refuel |
| `PWS_2001_78` | 56 | 57,362 | Refuel successful |
| `PWS_TEAMPIT01` | 57 | 58,366 | Teammate pits this lap - stay out |
| `PWS_TEAMPIT02` | 58 | 59,368 | Player can now pit |
| `PWS_2001_96` | 59 | 60,370 | Player rejoins track at X position |
| `PWS_2001_76` | 60 | 61,372 | Wheel gun failure |
| `PWS_2001_68` | 61 | 62,369 | Front Left tire change successful |
| `PWS_2001_72` | 62 | 63,358 | Slow Front Left tire change |
| `PWS_2001_69` | 63 | 64,357 | Front Right tire change successful |
| `PWS_2001_73` | 64 | 65,359 | Slow Front Right tire change |
| `PWS_2001_77` | 65 | 66,371 | Wheel is jammed |
| `PWS_2001_70` | 66 | 67,338 | Rear Left tire change successful |
| `PWS_2001_74` | 67 | 68,346 | Slow Rear Left tire change |
| `PWS_2001_71` | 68 | 69,349 | Rear Right tire change successful |
| `PWS_2001_75` | 69 | 70,339 | Slow Rear Right tire change |
| `PWS_RACEFAST03` | 70 | 71,343 | Player sets all-time lap record |
| `PWS_2001_102` | 72 | 73,344 | Player sets personal best lap |
| `PWS_MECHPROB16` | 74 | 75,345 | Damaged suspension - avoid kerbs |
| `PWS_MECHPROB14` | 75 | 76,348 | Electronics malfunction |
| `PWS_MECHPROB06` | 77 | 78,352 | Front Left puncture |
| `PWS_MECHPROB07` | 78 | 79,351 | Front Right puncture |
| `PWS_MECHPROB20` | 79 | 80,341 | Lost 1st gear |
| `PWS_MECHPROB21` | 80 | 81,340 | Lost 2nd gear |
| `PWS_MECHPROB22` | 81 | 82,342 | Lost 3rd gear |
| `PWS_MECHPROB23` | 82 | 83,353 | Lost 4th gear |
| `PWS_MECHPROB24` | 83 | 84,374 | Lost 5th gear |
| `PWS_MECHPROB25` | 84 | 85,375 | Lost 6th gear |
| `PWS_MECHPROB26` | 85 | 86,376 | Lost 7th gear |
| `PWS_MECHPROB27` | 86 | 87,377 | Stuck in 1st gear |
| `PWS_MECHPROB28` | 87 | 88,378 | Stuck in 2nd gear |
| `PWS_MECHPROB29` | 88 | 89,379 | Stuck in 3rd gear |
| `PWS_MECHPROB30` | 89 | 90,380 | Stuck in 4th gear |
| `PWS_MECHPROB31` | 90 | 91,381 | Stuck in 5th gear |
| `PWS_MECHPROB32` | 91 | 92,382 | Stuck in 6th gear |
| `PWS_MECHPROB33` | 92 | 93,383 | Stuck in 7th gear |
| `PWS_MECHPROB08` | 101 | 102,390,393 | Rear Left puncture |
| `PWS_MECHPROB09` | 102 | 103,387 | Rear Right puncture |
| `RE_PD_STALLED` | 111 | 112,405 | Engine stalled - T-car prepared |
| `RE_PD_STALLED_T` | 112 | 113,406 | T-car also stalled - session over |
| `PWS_OFFROAD02` | 117 | none | Player went off track (2) |
| `PWS_RETIRE03` | 119 | none | Player retired (generic 2) |
| `PWS_OVERTAKE02` | 123 | none | Player overtakes AI (2) |
| `PWS_OVERTAKE03` | 124 | none | Player overtakes AI (3) |
| `PWS_AILAPPIN05` | 125 | 121 | AI driver passes player |
| `PWS_AILAPPIN01` | 126 | 122,421 | AI driver approaches player |
| `PWS_OIL01` | 131 | 127,420 | Oil on track Sector 1 |
| `PWS_OIL02` | 132 | 128,417 | Oil on track Sector 2 |
| `PWS_OIL03` | 133 | 129,418 | Oil on track Sector 3 |
| `PWS_MECHANWARN0` | 134 | 130,415 | Meatball flag - must pit |
| `PWS_SCAR05` | 137 | 428 | Safety car in pit lane (2) |
| `PWS_SCAR02` | 139 | 425 | Safety car on track |
| `PWS_SCAR04` | 141 | none | Safety car in pit lane (1) |
| `PWS_PENALTY08` | 148 | 139 | Penalty (type 8) |
| `RE_PD_SG_DD` | 161 | 152,501 | Stop/Go penalty - dangerous driving |
| `PWS_2001_110` | 177 | none | Player wins race |
| `PWS_MISC30` | 195 | 183 | AI has pit stop issue |
| `PWS_2001_132` | 197 | 185 | AI driver jammed fuel hose |
| `PWS_2001_133` | 198 | 186 | AI driver stuck Front Left wheel |
| `PWS_2001_134` | 199 | 187 | AI driver stuck Front Right wheel |
| `PWS_2001_135` | 200 | 188 | AI driver stuck Rear Left wheel |
| `PWS_2001_136` | 201 | 189 | AI driver stuck Rear Right wheel |
| `PWS_AIPIT13` | 202 | 190 | AI rejoins track at X position |
| `RE_OD_INTER` | 203 | 191 | Other driver leaves pits on Inters |
| `RE_OD_SLICKS` | 205 | 193,508 | Other driver leaves pits on Slicks |
| `PWS_AIOFFTRACK0` | 237 | 224 | AI recovers from spin |
| `PWS_AIINCID06` | 240 | 227 | Car off track just ahead |
| `PWS_AIINCID03` | 241 | 228 | AI went off track |
| `PWS_AIINCID02` | 242 | none | AI went off track sector X |
| `PWS_AIINCID08` | 243 | 229 | Cars all over in sector X |
| `PWS_OD_HOLDING` | 244 | 230 | AI driver holds up another AI |
| `PWS_AIINCID07` | 245 | 231 | Car spun off just ahead |
| `PWS_AIINCID04` | 246 | 232 | AI spun off sector X |
| `PWS_AIINCID05` | 247 | none | AI spun off |
| `PWS_AIINCID01` | 248 | 233 | AI stalls on the grid |
| `PWS_2001_149` | 249 | 234 | AI driver held up by another AI |
| `PWS_AIPENALTY08` | 255 | 239 | AI penalty (type 8) |
| `PWS_AIPENALTY14` | 256 | 240 | AI penalty (type 14) |
| `PWS_AIPENALTY07` | 257 | 241 | AI stop/go penalty |
| `PWS_AIPENALTY05` | 258 | 242 | AI cutting track penalty |
| `PWS_MISC09` | 260 | 244 | AI blocking blue - investigation |
| `PWS_MISC48` | 261 | 245 | AI cutting track - investigation |
| `PWS_MISC06` | 262 | 246 | AI false start - investigation |
| `PWS_MISC08` | 263 | 247 | AI overtaking yellow - investigation |
| `PWS_MISC07` | 264 | 248 | AI speeding pits - investigation |
| `RE_OD_25_DD` | 265 | 249 | Other driver 25 sec penalty - dangerous driving |
| `RE_OD_INV_DD` | 266 | 250 | Other driver under investigation - dangerous driving |
| `RE_OD_SG_DD` | 267 | 251 | Other driver stop/go - dangerous driving |
| `PWS_AIPENALTY03` | 271 | 255 | AI overtaking yellow penalty |
| `PWS_AIBLUE02` | 273 | 257 | AI driver under blue flag - be ready |
| `PWS_AIBLUE01` | 274 | 258 | Player approaches AI driver (blue flag) |
| `PWS_AIFASTLAP03` | 280 | 263 | AI sets all-time fastest lap |
| `PWS_MISC29` | 282 | 265 | AI reduces pace (2) |
| `PWS_MISC28` | 283 | 266 | AI maintains position (2) |
| `PWS_MISC27` | 284 | 267 | AI maintains position (1) |
| `PWS_MISC26` | 285 | 268 | AI reduces pace (1) |
| `PWS_MISC25` | 286 | 269 | AI increases pace |
| `PWS_2001_165` | 287 | 270 | 75% race distance complete |
| `PWS_2001_164` | 288 | 271 | 50% race distance complete |
| `PWS_2001_163` | 292 | 274 | 25% race distance complete |
| `PWS_2001_221` | 298 | 280 | Rain starts - monsoon likely |
| `PWS_2001_222` | 299 | 281 | Rain eases off |
| `PWS_2001_218` | 300 | 282 | Overcast conditions - possible rain |
| `PWS_2001_220` | 301 | 283 | Rain starts - short shower |
| `PWS_2001_219` | 302 | none | Rain starts - light drizzle |
| `PWS_2001_311` | 306 | 285 | Fastest AI is on Inters |
| `PWS_2001_310` | 307 | 286 | Fastest AI is on Slicks |
| `PWS_2001_312` | 308 | 287 | Fastest AI is on Wets |
| `PWS_2001_309` | 309 | 288 | AI team uses driver orders |
| `PWS_2001_308` | 310 | 289 | You are being chased by AI driver |
| `RE_OD_FASTEST_M` | 311 | 290 | Other driver fastest - on Monsoon tires |
| `RE_PORSCHE_23` | 312 | none | Porsche Cup line 23 |
| `RE_PORSCHE_24` | 313 | none | Porsche Cup line 24 |
| `RE_PORSCHE_25` | 314 | none | Porsche Cup line 25 |
| `RE_PORSCHE_03` | 315 | none | Porsche Cup line 03 |

---

## UNKNOWN (104)

Marked SHIPPED, meaning "already plays in the stock game". That marking is not
trustworthy: only 57 of the 104 have both a confirmed hearing and a reachable
event. 37 are explicitly recorded as unverified. 10 are marked as playing while
also recorded as unreachable. 49 of the 104 were set by hand.

The only evidence of "vanilla" is one blog post (j21csblog, 2023-01-21). Nobody on
this project has listened to them. Treat as unknown until someone does.
