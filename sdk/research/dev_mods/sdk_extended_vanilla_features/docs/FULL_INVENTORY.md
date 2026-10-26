# Full inventory — all 304 voice lines

Per-clip reference list. For the summary and the per-function breakdown read
`docs/VOICE_LINES.md` first; this file exists so the 104 UNKNOWN clips can be
worked through one by one.

**The 104 'plays in vanilla' clips are NOT verified.** See VOICE_LINES.md.
Do not quote a 'usable today' total that counts them.

---|---:|---|
| **A — plays in vanilla** | 104 | Heard in the shipped game, no mod |
| **B — restored, proven** | 9 | Silent in vanilla, made audible by this project, verified by ear |
| **C — restorable** | 70 | Has a real emitter in the exe and is not cooldown-blocked. The emitter exists; the work is reaching it |
| **D — emitter but blocked** | 0 | — |
| **E — no emitter** | 108 | No code path emits this eid. Needs new emitting code, not just a trigger |
| **F — orphaned** | 13 | Audio file exists, no event maps to it at all |

**Total usable today: 113 of 304.**
**Reachable with trigger work: +70** → 183 (60%).

Caveat on tier C: "has an emitter" is derived from `emitter_index.json`, where
dynamic call sites are marked `?` — the eid is a *candidate* recovered from
literal assignments to the argument variable, not a proven emission. Treat a `?`
function as a lead to verify, not a guarantee.

---

## UNKNOWN — marked as playing in the stock game, NOT verified (104)

| Clip | SDT | EIDs | Description |
|---|---:|---|---|
| `PWS_QUAL07` | 1 | 3,294 | Pole sitter AI beats own time |
| `PWS_QUAL02` | 2 | 4,295 | AI driver set fastest Sector 1 |
| `PWS_QUAL03` | 3 | 5,296 | AI driver set fastest Sector 2 |
| `PWS_QUAL01` | 4 | 6,297 | AI driver is going out |
| `PWS_QUAL08` | 5 | 7,298 | AI driver sets initial lap time |
| `PWS_QUAL06` | 6 | 8,299 | AI driver moved up position |
| `PWS_2001_04` | 7 | 9,300 | AI driver fails to improve qualifying position |
| `PWS_QUAL23` | 8 | 10,301 | Pole sitter player beats own time |
| `PWS_QUAL15` | 11 | 13,304 | Player slower than pole Sector 1 |
| `PWS_QUAL16` | 12 | 14,305 | Player slower than pole Sector 2 |
| `PWS_2001_05` | 14 | 16,307 | Player is on Pole Position |
| `PWS_QUAL22` | 15 | 17,308 | Player moves up to X position |
| `PWS_QUAL21` | 16 | 18,309 | Player fails to improve position |
| `PWS_QUAL12` | 17 | 19,310 | Player faster than pole Sector 1 |
| `PWS_QUAL13` | 18 | 20,311 | Player faster than pole Sector 2 |
| `PWS_2001_16` | 19 | 21,312 | AI driver pits, completed X laps |
| `PWS_2001_21` | 20 | 22,313 | AI driver pits, all qualifying laps done |
| `PWS_2001_08` | 31 | 33,324 | AI driver is on Pole Position |
| `PWS_2001_09` | 32 | - | AI driver is on Pole Position (alt) |
| `RE_30_MINS_LEFT` | 33 | 34,325 | 30 minutes remain |
| `RE_45_MINS_LEFT` | 34 | 35,326 | 45 minutes remain |
| `PWS_POSUPDATE01` | 40 | 41,332 | Position update |
| `PWS_CHANGEPIT01` | 41 | 42,333 | Change pit instructions |
| `PWS_PIT02` | 51 | 52,360 | Pit crew is ready |
| `PWS_PIT14` | 54 | 55,364 | Pit this lap for Stop-Go |
| `PWS_RACEFAST02` | 71 | 72,354 | Player sets fastest race lap |
| `PWS_MECHPROB18` | 73 | 74,347 | Overheated brakes |
| `PWS_MECHPROB15` | 76 | 77,350 | Engine overheating |
| `RE_PD_FL_BAD_BL` | 93 | 94,384 | Front Left severe blistering |
| `PWS_TYRES02` | 103 | 104,399,400 | Severe blistering warning |
| `PWS_TYRES01` | 104 | 105,388 | Light blistering warning |
| `PWS_HIT10` | 113 | 114,407 | Player hits AI driver (3) |
| `PWS_HIT08` | 114 | 115,408 | Player hits AI driver (1) |
| `PWS_HIT09` | 115 | - | Player hits AI driver (2) |
| `PWS_OFFROAD01` | 116 | 116,409 | Player went off track (1) |
| `PWS_OVERTAKE01` | 122 | 120,413 | Player overtakes AI (1) |
| `PWS_AILAPPIN02` | 127 | 123,401 | Player under blue flag (1st) |
| `PWS_AILAPPIN03` | 128 | 124,422 | Player under blue flag (2nd) |
| `PWS_AILAPPIN04` | 129 | 125,414 | Player under blue flag (3rd) |
| `PWS_YELLOW01` | 143 | 134 | Yellow flag Sector 1 |
| `PWS_YELLOW02` | 145 | 136 | Yellow flag Sector 2 |
| `PWS_YELLOW03` | 147 | 138 | Yellow flag Sector 3 |
| `PWS_PENALTY14` | 149 | 140,491 | Penalty (type 14) |
| `PWS_PENALTY05` | 151 | 142,495 | Cutting track penalty |
| `PWS_MISC02` | 154 | 145,498 | Player false start - investigation |
| `RE_PD_SG_CUT` | 160 | 151,481 | Stop/Go penalty - cutting track |
| `PWS_PENALTY02` | 164 | 155,493 | Speeding in pits penalty |
| `PWS_WIN21` | 166 | 157,492 | Player finishes outside points |
| `PWS_WIN20` | 167 | 158,504 | Player finishes at X position |
| `PWS_2001_124` | 168 | 159,500 | Player wins WDC with X rounds to go |
| `RE_PD_WIN_EUR` | 169 | 160,488 | Player wins European GP |
| `PWS_WIN01` | 170 | 161,482 | Player wins Australian GP |
| `PWS_WIN09` | 171 | 162,483 | Player wins European GP |
| `PWS_WIN12` | 172 | 163,484 | Player wins German GP |
| `PWS_WIN02` | 173 | 164,485 | Player wins Malaysian GP |
| `PWS_WIN06` | 174 | 165,487 | Player wins Austrian GP |
| `PWS_WIN07` | 175 | 166,486 | Player wins Monaco GP |
| `PWS_WIN19` | 176 | 167 | Player wins generic Grand Prix |
| `PWS_WIN10` | 178 | 168 | Player wins French GP |
| `PWS_WIN08` | 179 | 169 | Player wins Canadian GP |
| `PWS_WIN11` | 180 | 170 | Player wins British GP |
| `PWS_WIN13` | 181 | 171,519 | Player wins Hungarian GP |
| `PWS_WIN16` | 182 | 172,517 | Player wins US GP |
| `PWS_WIN15` | 183 | 173 | Player wins Italian GP |
| `PWS_WIN04` | 184 | 174,518 | Player wins San Marino GP |
| `PWS_WIN03` | 185 | 175 | Player wins Brazilian GP |
| `PWS_WIN05` | 186 | 176 | Player wins Spanish GP |
| `PWS_WIN17` | 187 | 177 | Player wins Japanese GP |
| `PWS_AIPIT04` | 188 | 178 | AI driver pits |
| `PWS_AIPIT05` | 189 | - | AI driver pits (alt) |
| `PWS_AIPIT06` | 190 | 179 | Race leader pits |
| `PWS_AIPIT07` | 191 | - | AI driver pits (alt 2) |
| `PWS_AIPIT08` | 192 | 180 | AI has 1-stop strategy |
| `PWS_AIPIT09` | 193 | 181 | AI has 2-stop strategy |
| `PWS_AIPIT10` | 194 | 182 | AI has 3-stop strategy |
| `PWS_AIPIT01` | 208 | 196 | Team ready in the pits |
| `PWS_MISC14` | 210 | 198,512 | AI takes race lead (2) |
| `PWS_MISC12` | 211 | - | AI takes race lead (1) |
| `PWS_AIHIT08` | 233 | 220,477 | AI has suspension damage |
| `PWS_AIHIT07` | 234 | 221 | AI lost wheel |
| `PWS_AIHIT01` | 235 | 222 | AI lost front wing |
| `PWS_AIHIT02` | 236 | 223 | AI lost rear wing |
| `PWS_AIINCID10` | 238 | 225 | AI drivers collide (1) |
| `PWS_AIINCID11` | 239 | 226 | AI drivers collide (2) |
| `PWS_AIRETIRE05` | 250 | 235 | AI blown engine |
| `PWS_AIRETIRE03` | 251 | 236 | AI ran out of fuel |
| `PWS_AIRETIRE02` | 252 | 237 | AI retired (generic 2) |
| `PWS_AIRETIRE01` | 253 | - | AI retired (generic 1) |
| `PWS_AIRETIRE04` | 254 | 238 | AI suspension collapsed |
| `PWS_AIWIN02` | 276 | 260 | AI finishes X position (1) |
| `PWS_AIWIN03` | 277 | - | AI finishes X position (2) |
| `PWS_AIWIN01` | 278 | 261 | AI wins race |
| `PWS_AIFASTLAP01` | 279 | 262 | AI sets fastest lap of race |
| `PWS_MISC16` | 281 | 264 | Switch camera to AI driver |
| `PWS_2001_160` | 289 | 272 | X laps to go (short) |
| `PWS_2001_161` | 290 | - | X laps to go (long) |
| `PWS_2001_167` | 291 | 273 | Final lap of the race |
| `PWS_2001_156` | 294 | 276 | 10 minutes until session ends |
| `PWS_2001_155` | 295 | 277 | 15 minutes until session ends |
| `PWS_2001_158` | 296 | 278 | 3 minutes until session ends |
| `PWS_2001_157` | 297 | 279 | 5 minutes until session ends |
| `PWS_2001_224` | 303 | 284 | Player driving wrong way (2) |
| `PWS_2001_223` | 304 | - | Player driving wrong way (1) |
| `PWS_2001_225` | 305 | - | Player driving wrong way (3) |

---

## DONE — restored by this project, heard (9)

| Clip | SDT | EIDs | Description |
|---|---:|---|---|
| `PWS_HIT03` | 105 | 106,389 | Player lost Front Left wheel |
| `PWS_HIT04` | 106 | 107,373 | Player lost Front Right wheel |
| `PWS_HIT01` | 107 | 108,398 | Player lost front wing |
| `PWS_HIT05` | 108 | 109,402 | Player lost Rear Left wheel |
| `PWS_HIT06` | 109 | 110,403 | Player lost Rear Right wheel |
| `PWS_HIT02` | 110 | 111,404 | Player lost rear wing |
| `PWS_RETIRE02` | 118 | 117,410 | Player retired (generic 1) |
| `PWS_RED01` | 135 | 131,416 | Red flag - session stopped |
| `RE_PD_DQ` | 158 | 149,479 | Disqualified - failed to serve stop/go |

---

## NOT DONE — the game already has code for these (70)

| Clip | SDT | Emitter eid(s) | Emitter function | Description |
|---|---:|---|---|---|
| `PWS_QUAL32` | 0 | 293,397 | 0x0041b980? 0x0041c0f0? 0x0041d630? | Team front row lockout |
| `PWS_QUAL29` | 9 | 302 | 0x0041b980? | Some drivers on track |
| `PWS_QUAL28` | 10 | 303 | 0x0041b980? | Very few drivers on track |
| `PWS_QUAL30` | 13 | 306 | 0x0041b980? | Most drivers on track |
| `PWS_2001_17` | 21 | 314 | 0x0041b980? | AI driver starting 1st run |
| `PWS_2001_18` | 22 | 315 | 0x0041b980? | AI driver starting 2nd run |
| `PWS_2001_19` | 23 | 316 | 0x0041b980? | AI driver starting 3rd run |
| `PWS_2001_20` | 24 | 317 | 0x0041b980? | AI driver starting 4th run |
| `PWS_2001_22` | 25 | 318 | 0x0041b980? | Rain expected - make run ASAP |
| `PWS_2001_24` | 26 | 319 | 0x0041b980? | Rain expected to ease - wait |
| `PWS_2001_25` | 27 | 320 | 0x0041b980? | Rain expected to continue - make run ASAP |
| `PWS_2001_14` | 28 | 321 | 0x0041b980? | AI driver ahead on Flying Lap |
| `PWS_2001_15` | 29 | 322 | 0x0041b980? | AI driver ahead on In Lap |
| `PWS_2001_13` | 30 | 323 | 0x0041b980? | AI driver ahead on Out Lap |
| `RE_OD_EXIT_RED` | 35 | 327 | 0x0041b980? | Other driver under investigation - exiting red |
| `RE_OD_LAP_DISAL` | 36 | 328 | 0x0041b980? | Other driver lap disallowed - exited under red |
| `RE_PD_EXIT_RED` | 37 | 329 | 0x0041b980? | Under investigation - exiting under red |
| `RE_PD_LAP_DISAL` | 38 | 330 | 0x0041b980? | Fastest lap disallowed - exited under red |
| `PWS_2001_33` | 39 | 331 | 0x0041b980? | X position or better secures WDC |
| `PWS_PIT12` | 42 | 334 | 0x0041b980? | Limiter Off - clear of pit lane |
| `PWS_PIT09` | 43 | 335 | 0x0041b980? | Gear (pit procedure) |
| `PWS_PIT10` | 44 | 336 | 0x0041b980? | Go (pit procedure) |
| `PWS_2001_80` | 53 | 337 | 0x0041c3e0 | Engine stalled - will get refired |
| `RE_PD_FL_BLIST` | 94 | 95 | 0x0041aa10 | Front Left light blistering |
| `RE_PD_FR_BAD_BL` | 95 | 96 | 0x0041ab30 | Front Right severe blistering |
| `RE_PD_FR_BLIST` | 96 | 97 | 0x0041aa10 | Front Right light blistering |
| `RE_PD_RL_BAD_BL` | 97 | 98 | 0x0041ab30 | Rear Left severe blistering |
| `RE_PD_RL_BLIST` | 98 | 99 | 0x0041aa10 | Rear Left light blistering |
| `RE_PD_RR_BAD_BL` | 99 | 100 | 0x0041ab30 | Rear Right severe blistering |
| `RE_PD_RR_BLIST` | 100 | 101 | 0x0041aa10 | Rear Right light blistering |
| `PWS_RETIRE01` | 120 | 118 | 0x0041b250 | Player engine stall |
| `PWS_RETIRE04` | 121 | 119 | 0x0041b250 | Player blown engine |
| `PWS_GREEN01` | 130 | 126 | 0x0041b3c0 | Green flag conditions |
| `PWS_SLOWCAR01` | 136 | 427 | 0x0041e320 | Slow car ahead |
| `PWS_SCAR01` | 138 | 423 | 0x00420820 | Safety car deployed - exiting pits |
| `PWS_SCAR03` | 140 | 424 | 0x00420820 | Safety car ending - green this lap |
| `PWS_YELLOW04` | 142 | 133 | 0x0041b3c0 | Sector 1 cleared |
| `PWS_YELLOW05` | 144 | 135 | 0x0041b3c0 | Sector 2 cleared |
| `PWS_YELLOW06` | 146 | 137 | 0x0041b3c0 | Sector 3 cleared |
| `PWS_PENALTY07` | 150 | 490 | 0x0041daf0 | Stop/go penalty |
| `PWS_MISC05` | 152 | 494 | 0x0041daf0 | Player blocking blue - investigation |
| `PWS_MISC47` | 153 | 499 | 0x0041daf0 | Player cutting track - investigation |
| `PWS_MISC04` | 155 | 503 | 0x0041daf0 | Player overtaking yellow - investigation |
| `PWS_MISC03` | 156 | 502 | 0x0041daf0 | Player speeding pits - investigation |
| `RE_PD_25_DD` | 157 | 478 | 0x0041daf0 | 25 sec penalty - dangerous driving |
| `RE_PD_INV_DD` | 159 | 480 | 0x0041daf0 | Under investigation - dangerous driving |
| `RE_PD_SG_SPEED` | 162 | 153 | 0x0041a720? | Stop/Go penalty - speeding in pits |
| `PWS_PENALTY04` | 163 | 154 | 0x0041a720? | Blocking blue penalty |
| `PWS_PENALTY03` | 165 | 496 | 0x00420cf0 | Overtaking yellow penalty |
| `PWS_2001_131` | 196 | 516 | 0x0041c0f0? | AI driver engine stalled in pits |
| `RE_OD_MONSOON` | 204 | 511 | 0x0041be60 | Other driver leaves pits on Monsoons |
| `RE_OD_WET` | 206 | 509 | 0x0041be60 | Other driver leaves pits on Wets |
| `PWS_AIPIT11` | 207 | 510 | 0x0041be60 | AI pits to serve stop-go penalty |
| `PWS_MISC46` | 209 | 507 | 0x0041be60 | AI driver laps another AI |
| `PWS_2001_141` | 212 | 513 | 0x0041be60 | Team positioned to secure WCC |
| `PWS_2001_138` | 213 | 506 | 0x0041be60 | AI driver positioned to secure WDC |
| `PWS_AIMECHPROB1` | 221 | 208,449,450,451,452,453 | 0x0041ac50? 0x0041d630? |  |
| `RE_OD_LOST_7` | 222 | 457 | 0x0041d630? | Other driver lost 7th gear |
| `RE_OD_PUNCTURE` | 223 | 458 | 0x0041d630? | Other driver has a puncture |
| `RE_OD_STUCK_7` | 224 | 211,462 | 0x0041ac50? 0x0041d630? | Other driver stuck in 7th gear |
| `PWS_AIMECHPROB0` | 230 | 212,213,214,215,216,217,447,448,463,464,465,466,467,471 | 0x0041ac50? 0x0041d630? |  |
| `PWS_AITYRES02` | 231 | 472 | 0x0041d630? | AI severe blistering |
| `PWS_AITYRES01` | 232 | 473 | 0x0041d630? | AI light blistering |
| `PWS_AIPENALTY22` | 259 | 243 | 0x0041a940? | AI penalty (type 22) |
| `PWS_AIPENALTY04` | 268 | 252 | 0x0041a720? | AI blocking blue penalty |
| `PWS_AIPENALTY13` | 269 | 253 | 0x0041a720? | AI penalty (type 13) |
| `PWS_AIPENALTY02` | 270 | 254 | 0x0041a720? | AI speeding in pits penalty |
| `PWS_AIPENALTY01` | 272 | 256 | 0x0041a720? | AI penalty (type 1) |
| `PWS_AIBLUE03` | 275 | 259 | 0x00419120 | Player passes AI driver |
| `PWS_2001_159` | 293 | 275 | 0x0041a6b0 | Session ends |

---

## NOT DONE — the game has no code for these (108)

| Clip | SDT | EIDs | Description |
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
| `PWS_AILAPPIN05` | 125 | 121 | AI driver passes player |
| `PWS_AILAPPIN01` | 126 | 122,421 | AI driver approaches player |
| `PWS_OIL01` | 131 | 127,420 | Oil on track Sector 1 |
| `PWS_OIL02` | 132 | 128,417 | Oil on track Sector 2 |
| `PWS_OIL03` | 133 | 129,418 | Oil on track Sector 3 |
| `PWS_MECHANWARN0` | 134 | 130,415 | Meatball flag - must pit |
| `PWS_SCAR05` | 137 | 428 | Safety car in pit lane (2) |
| `PWS_SCAR02` | 139 | 425 | Safety car on track |
| `PWS_PENALTY08` | 148 | 139 | Penalty (type 8) |
| `RE_PD_SG_DD` | 161 | 152,501 | Stop/Go penalty - dangerous driving |
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
| `PWS_AIINCID08` | 243 | 229 | Cars all over in sector X |
| `PWS_OD_HOLDING` | 244 | 230 | AI driver holds up another AI |
| `PWS_AIINCID07` | 245 | 231 | Car spun off just ahead |
| `PWS_AIINCID04` | 246 | 232 | AI spun off sector X |
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
| `PWS_2001_311` | 306 | 285 | Fastest AI is on Inters |
| `PWS_2001_310` | 307 | 286 | Fastest AI is on Slicks |
| `PWS_2001_312` | 308 | 287 | Fastest AI is on Wets |
| `PWS_2001_309` | 309 | 288 | AI team uses driver orders |
| `PWS_2001_308` | 310 | 289 | You are being chased by AI driver |
| `RE_OD_FASTEST_M` | 311 | 290 | Other driver fastest - on Monsoon tires |

---

## ORPHANED — audio with no event mapping (13)

| Clip | SDT | EIDs | Description |
|---|---:|---|---|
| `PWS_OFFROAD02` | 117 | - | Player went off track (2) |
| `PWS_RETIRE03` | 119 | - | Player retired (generic 2) |
| `PWS_OVERTAKE02` | 123 | - | Player overtakes AI (2) |
| `PWS_OVERTAKE03` | 124 | - | Player overtakes AI (3) |
| `PWS_SCAR04` | 141 | - | Safety car in pit lane (1) |
| `PWS_2001_110` | 177 | - | Player wins race |
| `PWS_AIINCID02` | 242 | - | AI went off track sector X |
| `PWS_AIINCID05` | 247 | - | AI spun off |
| `PWS_2001_219` | 302 | - | Rain starts - light drizzle |
| `RE_PORSCHE_23` | 312 | - | Porsche Cup line 23 |
| `RE_PORSCHE_24` | 313 | - | Porsche Cup line 24 |
| `RE_PORSCHE_25` | 314 | - | Porsche Cup line 25 |
| `RE_PORSCHE_03` | 315 | - | Porsche Cup line 03 |
