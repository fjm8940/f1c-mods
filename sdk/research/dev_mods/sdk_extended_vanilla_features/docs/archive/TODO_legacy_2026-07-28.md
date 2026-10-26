# Extended Vanilla Features — Practical TODO

> 520 events, 316 SDT clips. 26 BLOCKED (FLT_MAX cooldown). 23 ORPHANED (no event).
> EVENT_TABLE = 0x5BF9A0 (stride 0x14). CONTROLLER = 0x5EA618.
> Injection tool: `python commentary_inject.py <eid>`

---

## ⚠ CORRECTIONS — verified 2026-07-27, read before working any row below

A 20-worker verification pass (`tools/evf_batch.py`, 233/236 rows machine-validated
against `data/full_event_dump.json` + `data/progress.json`) plus live disassembly and
in-race memory reads found the tables below to be wrong in four ways. Details and
repro lines are in the corpus root `findings.jsonl`.

**1. ~57 rows below are already SHIPPED.** They are audible in vanilla and need no
work. `data/progress.json` is the authority on status, not this file. Verified
examples: `CI-149` (2001_05 "you're on pole" — ships via **eid 307**, not eid 16 as
listed), `CI-158`, `CI-161`, `CI-178`, `CI-181`, `CI-193`, `CI-211`, `CI-021`.
Check `progress.json` before starting any row.

**2. The "guard never fires" clips are RACE-ONLY, not broken.** `SessionType`
(`0x007C56A4`) is **5 in qualifying and 7 in a race** (both live-verified). Eight
emitter files gate on `SessionType == 7`. So `GREEN01`, `Red01`, `Yellow05`,
`AILappin04`, `AIBlue03`, `AIRetire05`, `RE_OD_STUCK_7`, `AIPit08/09/10` are simply
race commentary. **Do not patch these guards** — test in a race first.
(`docs/38-confirmed-offsets.md`'s old `2=qual` was wrong and has been corrected.)

**3. The stuck-gear rows are off by one.** `MechProb29` is **3rd** gear, not 4th;
`MechProb30` is 4th. The whole `MechProb26-33` block is likely shifted — re-derive
descriptions from `progress.json` before wiring any of them.

**4. `MechWarn0` (CI-121) is really `PWS_MECHANWARN0`** (MECH**AN**WARN) in the SDT.

**Genuinely blocked, mechanism known:** `AIPit08`/`AIPit09` (eids 180/181) can never
fire because `vehicle+0x342C` (`aiPitStrategyIndex`) reads **3 on every car**, so
`AIFormationLap`'s switch always selects case 3 → eid 182. That is the root cause of
the "AIPit10 fires on 1-stoppers" bug. The offset has no writer anywhere in the
decompiled corpus — fixing it means finding what should write it, not patching
`AIFormationLap`.

---

## TRIVIAL — 1 command, immediate result

### Config Flips

`.plr` is a **plain-text INI-style file**, not a binary struct. Verified on
`Save/<name>/<name>.PLR` (33,873 bytes, opens `//****`). Edit the `Key="value"`
text; there is no byte offset to poke.

> The `+0xAF0 / +0x6E5 / +0x1A30` numbers an earlier pass listed here were the
> **in-memory** offsets the parser writes into (`config_schema.json`), NOT file
> offsets. Writing bytes at those file positions corrupts the .plr.

Close the game first — it rewrites the .plr on exit and will overwrite your edit.

`Formation Lap` exists **once per session mode**, all under `[ Race Conditions ]`:
`CURNT / PRACT / QUICK / GPRIX / CHAMP / MULTI / RPLAY`. Flip the mode you
actually play (`GPRIX` for a one-off race weekend, `CHAMP` for a season);
`CURNT` is the live copy the game reads for the session in progress.

| # | Section | Key | Old | New | What it does |
|---|---------|-----|-----|-----|-------------|
| CF-01 | `[ Race Conditions ]` | `GPRIX Formation Lap` (line ~91) | `"0"` | `"1"` | Formation lap before race. AI grid-following path exists |
| CF-02 | `[ Graphic Options ]` | `Night Lighting` (line ~213) | `"1.00000"` | `"0.50000"` | 1.0 = disabled. Lower = night skybox/light models |
| CF-03 | `[ Race Conditions ]` | `Run Practice2` (line 64) | `"0"` | `"1"` | Second practice session |
| CF-04 | `[ Race Conditions ]` | `Run Practice3` (line 65) | `"0"` | `"1"` | Third practice session |
| CF-05 | `[ Race Conditions ]` | `Run Practice4` (line 66) | `"0"` | `"1"` | Fourth practice session |

Supporting keys already present, worth reading before CF-01:
`AI Follow Grid="1"` (line 70), `Recon Pit Open="300"` (line 71).

**Unproven:** these keys parse, but nothing here proves the session
state machine honours them — `Formation Lap` and `Run Practice2/3/4` are exactly
the features README lists as "config exists, no state-machine transition". Flip
them, launch, and record the result in `findings.jsonl` either way. A flip that
does nothing is a DISPROVEN worth writing down.

### Commentary Injection — ALIVE (wired, just silent)

All clip names use `PWS_*.mp2` or `RE_*.mp2` prefix. Command: `python commentary_inject.py <eid>` from game. MUST be on track, commentary enabled.

#### Pit Stop Procedure (16 clips — emitters never fire, PitSelectionStateMachine skips them)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-001 | 2001_68 | 62 | "Tire change successful — Front Left" | Inject eid 62 |
| CI-002 | 2001_69 | 64 | "Tire change successful — Front Right" | Inject eid 64 |
| CI-003 | 2001_70 | 67 | "Tire change successful — Rear Left" | Inject eid 67 |
| CI-004 | 2001_71 | 69 | "Tire change successful — Rear Right" | Inject eid 69 |
| CI-005 | 2001_72 | 63 | "Slow tire change — Front Left" | Inject eid 63 |
| CI-006 | 2001_73 | 65 | "Slow tire change — Front Right" | Inject eid 65 |
| CI-007 | 2001_74 | 68 | "Slow tire change — Rear Left" | Inject eid 68 |
| CI-008 | 2001_75 | 70 | "Slow tire change — Rear Right" | Inject eid 70 |
| CI-009 | 2001_76 | 61 | "Wheel gun failure" | Inject eid 61 |
| CI-010 | 2001_77 | 66 | "Wheel jammed" | Inject eid 66 |
| CI-011 | 2001_78 | 57 | "Successful refuel" | Inject eid 57 |
| CI-012 | 2001_79 | 56 | "Fuel hose issue — slow refuel" | Inject eid 56 |
| CI-013 | 2001_80 | 54 | "Stalled engine, we'll get you refired" | Inject eid 54 |
| CI-014 | Pit06 | 51 | "Limiter On" | Inject eid 51 |
| CI-015 | Pit12 | 43 | "Limiter Off, clear of pit lane" | Inject eid 43 |
| CI-016 | Pit13 | 48 | "Countdown 10 to 1 for stop-go" | Inject eid 48 |

#### Session Phase (8 clips — no distance-tracker emitter)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-017 | 2001_163 | 274 | "25% race distance complete" | Inject eid 274 |
| CI-018 | 2001_164 | 271 | "50% race distance complete" | Inject eid 271 |
| CI-019 | 2001_165 | 270 | "75% race distance complete" | Inject eid 270 |
| CI-020 | 2001_167 | 273 | "Final lap of the race" | Inject eid 273 |
| CI-021 | 2001_155 | 277 | "15 minutes until session ends" | Inject eid 277 |
| CI-022 | 2001_156 | 276 | "10 minutes until session ends" | Inject eid 276 |
| CI-023 | 2001_157 | 279 | "5 minutes until session ends" | Inject eid 279 |
| CI-024 | 2001_158 | 278 | "3 minutes until session ends" | Inject eid 278 |

#### Weather Commentary (6 clips — no weather→commentary bridge)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-025 | 2001_218 | 282 | "Overcast conditions, possible rain" | Inject eid 282 |
| CI-026 | 2001_220 | 283 | "Rain starts, won't be more than a shower" | Inject eid 283 |
| CI-027 | 2001_221 | 280 | "Rain starts, probably will continue (monsoon)" | Inject eid 280 |
| CI-028 | 2001_222 | 281 | "Rain eases off" | Inject eid 281 |
| CI-029 | 2001_310 | 286 | "Fastest AI on Slicks" | Inject eid 286 |
| CI-030 | 2001_311 | 285 | "Fastest AI on Inters" | Inject eid 285 |

#### Qualifying Detail (10 clips — run counter not hooked)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-031 | 2001_13 | 32 | "AI Driver ahead, on Out Lap" | Inject eid 32 |
| CI-032 | 2001_14 | 30 | "AI Driver ahead, on Flying Lap" | Inject eid 30 |
| CI-033 | 2001_15 | 31 | "AI Driver ahead, on In Lap" | Inject eid 31 |
| CI-034 | 2001_17 | 23 | "AI Driver starting 1st run" | Inject eid 23 |
| CI-035 | 2001_18 | 24 | "AI Driver starting 2nd run" | Inject eid 24 |
| CI-036 | 2001_19 | 25 | "AI Driver starting 3rd run" | Inject eid 25 |
| CI-037 | 2001_20 | 26 | "AI Driver starting 4th run" | Inject eid 26 |
| CI-038 | 2001_22 | 27 | "Rain expected, make qualifying run ASAP" | Inject eid 27 |
| CI-039 | 2001_24 | 28 | "Rain expected to ease, wait" | Inject eid 28 |
| CI-040 | 2001_25 | 29 | "Rain expected to continue, make run ASAP" | Inject eid 29 |

#### Championship / WDC (6 clips — conditions rare)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-041 | 2001_33 | 40 | "X position or better secures WDC" | Inject eid 40 |
| CI-042 | 2001_138 | 200 | "AI Driver is positioned to secure WDC" | Inject eid 200 |
| CI-043 | 2001_141 | 199 | "Team is positioned to secure WCC" | Inject eid 199 |
| CI-044 | 2001_308 | 289 | "You are now being chased by AI Driver 1" | Inject eid 289 |
| CI-045 | 2001_309 | 288 | "AI Team uses driver orders" | Inject eid 288 |
| CI-046 | 2001_149 | 234 | "AI Driver is held up by another AI Driver" | Inject eid 234 |

#### Player Mechanical — Punctures (4 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-047 | MechProb06 | 78 | "Puncture — Front Left" | Inject eid 78 |
| CI-048 | MechProb07 | 79 | "Puncture — Front Right" | Inject eid 79 |
| CI-049 | MechProb08 | 102 | "Puncture — Rear Left" | Inject eid 102 |
| CI-050 | MechProb09 | 103 | "Puncture — Rear Right" | Inject eid 103 |

#### Player Mechanical — Other (4 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-051 | MechProb14 | 76 | "Electronics malfunction" | Inject eid 76 |
| CI-052 | MechProb15 | 77 | "Engine overheating" | Inject eid 77 |
| CI-053 | MechProb16 | 75 | "Damaged suspension, avoid kerbs" | Inject eid 75 |
| CI-054 | MechProb18 | 74 | "Overheated brakes" | Inject eid 74 |

#### Per-Tire Blistering (8 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-055 | RE_PD_FL_BAD_BL | 94 | "Severe blistering — Front Left" | Inject eid 94 |
| CI-056 | RE_PD_FL_BLIST | 95 | "Light blistering — Front Left" | Inject eid 95 |
| CI-057 | RE_PD_FR_BAD_BL | 96 | "Severe blistering — Front Right" | Inject eid 96 |
| CI-058 | RE_PD_FR_BLIST | 97 | "Light blistering — Front Right" | Inject eid 97 |
| CI-059 | RE_PD_RL_BAD_BL | 98 | "Severe blistering — Rear Left" | Inject eid 98 |
| CI-060 | RE_PD_RL_BLIST | 99 | "Light blistering — Rear Left" | Inject eid 99 |
| CI-061 | RE_PD_RR_BAD_BL | 100 | "Severe blistering — Rear Right" | Inject eid 100 |
| CI-062 | RE_PD_RR_BLIST | 101 | "Light blistering — Rear Right" | Inject eid 101 |

#### AI Incidents (7 clips — sub_41A8C0 switch covers only 4 of 11 types)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-063 | AIIncid01 | 233 | "AI stalls on the grid" | Inject eid 233 |
| CI-064 | AIIncid03 | 228 | "Went off track — generic" | Inject eid 228 |
| CI-065 | AIIncid04 | 232 | "Spun off in specific sector" | Inject eid 232 |
| CI-066 | AIIncid06 | 227 | "Went off track just ahead" | Inject eid 227 |
| CI-067 | AIIncid07 | 231 | "Spun off just ahead" | Inject eid 231 |
| CI-068 | AIIncid08 | 229 | "Cars all over the place in sector X" | Inject eid 229 |
| CI-069 | AIIncid10 | 225 | "AI Drivers collide (variant 1)" | Inject eid 225 |

#### Blue Flag / Lapping Chain (5 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-070 | AIBlue01 | 258 | "Player approaches AI Driver" | Inject eid 258 |
| CI-071 | AIBlue02 | 257 | "AI Driver under blue flag, be ready" | Inject eid 257 |
| CI-072 | AIBlue03 | 259 | "Player passes AI Driver" | Inject eid 259 |
| CI-073 | AILappin01 | 122 | "AI Driver approaches player (blue flag)" | Inject eid 122 |
| CI-074 | AILappin05 | 121 | "AI Driver passes player" | Inject eid 121 |

#### Penalty System — Self (8 clips, penalty emitters exist but rare)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-075 | MISC_02 | 145 | "False start under investigation" | Inject eid 145 |
| CI-076 | MISC_03 | 147 | "Speeding in pits under investigation" | Inject eid 147 |
| CI-077 | MISC_04 | 146 | "Overtaking yellow under investigation" | Inject eid 146 |
| CI-078 | MISC_05 | 143 | "Blocking blue under investigation" | Inject eid 143 |
| CI-079 | MISC_47 | 144 | "Cutting track investigation — Player" | Inject eid 144 |
| CI-080 | RE_PD_25_DD | 148 | "Player issued 25 sec penalty (dangerous driving)" | Inject eid 148 |
| CI-081 | RE_PD_INV_DD | 150 | "Player under investigation (dangerous driving)" | Inject eid 150 |
| CI-082 | RE_PD_EXIT_RED | 38 | "Player under investigation (exiting under red)" | Inject eid 38 |

#### Penalty System — AI (7 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-083 | MISC_06 | 246 | "AI false start under investigation" | Inject eid 246 |
| CI-084 | MISC_07 | 248 | "AI speeding in pits" | Inject eid 248 |
| CI-085 | MISC_08 | 247 | "AI overtaking yellow" | Inject eid 247 |
| CI-086 | MISC_09 | 244 | "AI blocking blue" | Inject eid 244 |
| CI-087 | MISC_48 | 245 | "Cutting track investigation — AI" | Inject eid 245 |
| CI-088 | RE_OD_25_DD | 249 | "Other driver issued 25 sec penalty" | Inject eid 249 |
| CI-089 | RE_OD_INV_DD | 250 | "Other driver under investigation (dangerous driving)" | Inject eid 250 |

#### Pit Strategy / AI Pit / Misc (15 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-090 | AIPit08 | 180 | "AI Driver has 1 stop strategy" | Inject eid 180 |
| CI-091 | AIPit09 | 181 | "AI Driver has 2 stop strategy" | Inject eid 181 |
| CI-092 | AIPit11 | 195 | "AI Driver pits to serve stop-go penalty" | Inject eid 195 |
| CI-093 | AIPit13 | 190 | "AI Driver rejoins track at X position" | Inject eid 190 |
| CI-094 | MISC_18 | 47 | "Engineer asks player to stop next lap" | Inject eid 47 |
| CI-095 | MISC_25 | 269 | "AI increases pace" | Inject eid 269 |
| CI-096 | MISC_26 | 268 | "AI reduces pace (variant 1)" | Inject eid 268 |
| CI-097 | MISC_29 | 265 | "AI reduces pace (variant 2)" | Inject eid 265 |
| CI-098 | MISC_27 | 267 | "AI maintains position" | Inject eid 267 |
| CI-099 | MISC_28 | 266 | "AI holds position" | Inject eid 266 |
| CI-100 | MISC_30 | 183 | "AI has pit stop issue" | Inject eid 183 |
| CI-101 | MISC_46 | 197 | "AI laps AI" | Inject eid 197 |
| CI-102 | MISC_54 | 46 | "Box! Engineer asks to box" | Inject eid 46 |
| CI-103 | OD_Holding | 230 | "AI holds up another AI" | Inject eid 230 |
| CI-104 | Oil01 | 127 | "Oil on track — Sector 1" | Inject eid 127 |

#### AI Mechanical (18 clips — emitter exists for wing/wheel but not gear)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-105 | AIHit01 | 222 | "AI lost front wing" | Inject eid 222 |
| CI-106 | AIHit02 | 223 | "AI lost rear wing" | Inject eid 223 |
| CI-107 | AIHit07 | 221 | "AI lost wheel" | Inject eid 221 |
| CI-108 | AIHit08 | 220 | "AI has suspension issue" | Inject eid 220 |
| CI-109 | AIMechProb lost 1-6 + stuck 1-6 | 201-208 | "AI lost gear X / stuck in gear X" | Inject eid 201-208 |
| CI-110 | RE_OD_LOST_7 | 209 | "Other driver lost 7th gear" | Inject eid 209 |
| CI-111 | RE_OD_STUCK_7 | 211 | "Other driver stuck in 7th gear" | Inject eid 211 |
| CI-112 | RE_OD_PUNCTURE | 210 | "Other driver has a puncture" | Inject eid 210 |

#### AI Tire Choice (4 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-113 | RE_OD_INTER | 191 | "Other driver leaves pits, using Inters" | Inject eid 191 |
| CI-114 | RE_OD_MONSOON | 192 | "Other driver leaves pits, using Monsoon tires" | Inject eid 192 |
| CI-115 | RE_OD_SLICKS | 193 | "Other driver leaves pits, using Slicks" | Inject eid 193 |
| CI-116 | RE_OD_WET | 194 | "Other driver leaves pits, using Wets" | Inject eid 194 |

#### Session / Event Clips (5 clips)

| # | Clip | EID | Engineer says | Action |
|---|------|----:|---------------|--------|
| CI-117 | 2001_102 | 73 | "Player sets personal best lap" | Inject eid 73 |
| CI-118 | 2001_159 | 275 | "Session ends" | Inject eid 275 |
| CI-119 | RaceFast03 | 71 | "Player sets all-time fastest lap on track" | Inject eid 71 |
| CI-120 | SlowCar01 | 132 | "Slow car on track ahead of player" | Inject eid 132 |
| CI-121 | MechanWarn0 | 130 | "Meatball flag — must pit (mechanical/stewards)" | Inject eid 130 |

#### Remaining ALIVE Clips Quick-Reference

| # | Clip | EID | Engineer says |
|---|------|----:|---------------|
| CI-122 | Teampit01 | 58 | "Teammate will pit on current lap, stay out" |
| CI-123 | Teampit02 | 59 | "Player can now pit" |
| CI-124 | ChangePit | 42 | "Change pit instructions / request pit" |
| CI-125 | PosUpdate01 | 41 | "Position update" |
| CI-126 | Pit01 | 49 | "Pit on this lap" |
| CI-127 | Pit02 | 52 | "Pit crew is ready" |
| CI-128 | Pit03 | 50 | "Watch for pit lane entrance" |
| CI-129 | Pit08 | 53 | "Brake (pit procedure)" |
| CI-130 | Pit09 | 44 | "Gear (pit procedure)" |
| CI-131 | Pit10 | 45 | "Go (pit procedure)" |
| CI-132 | Pit14 | 55 | "Pit this lap for Stop-Go penalty" |
| CI-133 | 2001_124 | 159 | "Player wins WDC with X rounds to go" |
| CI-134 | RE_30_MINS_LEFT | 34 | "30 minutes remaining in session" |
| CI-135 | RE_45_MINS_LEFT | 35 | "45 minutes remaining in session" |
| CI-136 | GREEN01 | 126 | "Green flag conditions" |
| CI-137 | Yellow01 | 134 | "Yellow flag deployed Sector 1" |
| CI-138 | Yellow02 | 136 | "Yellow flag deployed Sector 2" |
| CI-139 | Yellow03 | 138 | "Yellow flag deployed Sector 3" |
| CI-140 | Yellow04 | 133 | "Sector 1 cleared" |
| CI-141 | Yellow05 | 135 | "Sector 2 cleared" |
| CI-142 | Yellow06 | 137 | "Sector 3 cleared" |
| CI-143 | Red01 | 131 | "Red flag deployed, session stopped" |
| CI-144 | Scar01 | 423 | "Safety car deployed, exiting pits" |
| CI-145 | Scar02 | 425 | "Safety car on track" |
| CI-146 | Scar03 | 424 | "Safety car ending, going green at end of lap" |
| CI-147 | Scar05 | 428 | "Safety car in pit lane (alt)" |
| CI-148 | 2001_04 | 9 | "AI Driver fails to improve Qualifying position" |
| CI-149 | 2001_05 | 16 | "Player is on Pole Position" |
| CI-150 | 2001_08 | 33 | "AI Driver is on Pole Position" |
| CI-151 | 2001_16 | 21 | "AI Driver pits, completed X amount of laps" |
| CI-152 | 2001_21 | 22 | "AI Driver pits, completed all qualifying laps" |
| CI-153 | 2001_96 | 60 | "Player rejoins track at X position after pit stop" |
| CI-154 | 2001_131-136 | 184-189 | "AI pit stop issues (stall, fuel, wheels)" |
| CI-155 | AIPit01 | 196 | "AI team ready in the pits" |
| CI-156 | AIPit04 | 178 | "AI Driver pits" |
| CI-157 | AIPit06 | 179 | "Race leader pits" |
| CI-158 | AIFastLap01 | 262 | "AI sets fastest lap of race" |
| CI-159 | AIFastLap03 | 263 | "AI sets all-time fastest lap on track" |
| CI-160 | AIRetire02 | 237 | "AI straight up retired" |
| CI-161 | AIRetire03 | 236 | "AI ran out of fuel" |
| CI-162 | AIRetire04 | 238 | "AI suspension collapsed" |
| CI-163 | AIRetire05 | 235 | "AI blown engine" |
| CI-164 | AITyres01 | 219 | "Light blistering — AI" |
| CI-165 | AITyres02 | 218 | "Severe blistering — AI" |
| CI-166 | AIWin01 | 261 | "AI wins race" |
| CI-167 | AIWin02 | 260 | "AI finishes at X position" |
| CI-168 | Offroad01 | 116 | "Player goes off track" |
| CI-169 | Overtake01 | 120 | "Player overtakes AI Driver" |
| CI-170 | Hit08 | 115 | "Player hits AI Driver (variant 1)" |
| CI-171 | Hit10 | 114 | "Player hits AI Driver (variant 2)" |
| CI-172 | Tyres01 | 105 | "Light blistering — Player" |
| CI-173 | Tyres02 | 104 | "Severe blistering — Player" |
| CI-174 | Qual01 | 6 | "AI Driver is going out" |
| CI-175 | Qual02 | 4 | "AI Driver set fastest sector 1" |
| CI-176 | Qual03 | 5 | "AI Driver set fastest sector 2" |
| CI-177 | Qual06 | 8 | "AI Driver moved up to X position" |
| CI-178 | Qual07 | 3 | "Pole sitter AI beats own provisional pole" |
| CI-179 | Qual08 | 7 | "AI Driver sets initial laptime" |
| CI-180 | Qual12 | 19 | "Player faster than pole at sector 1" |
| CI-181 | Qual13 | 20 | "Player faster than pole at sector 2" |
| CI-182 | Qual15 | 13 | "Player slower than pole at sector 1" |
| CI-183 | Qual16 | 14 | "Player slower than pole at sector 2" |
| CI-184 | Qual21 | 18 | "Player fails to improve qualifying position" |
| CI-185 | Qual22 | 17 | "Player moves up to X position" |
| CI-186 | Qual23 | 10 | "Pole sitter player beats own provisional pole" |
| CI-187 | Qual28 | 12 | "Very few drivers on track" |
| CI-188 | Qual29 | 11 | "Some drivers on track" |
| CI-189 | Qual30 | 15 | "Most drivers on track" |
| CI-190 | Qual32 | 293 | "Team front row lockout" |
| CI-191 | RaceFast02 | 72 | "Player sets fastest lap of the race" |
| CI-192 | 2001_110 | n/a | Player wins race (orphaned — see ORPHANED section) |
| CI-193 | AIPit10 | 182 | "AI Driver has 3 stop strategy" (scripted wrong) |
| CI-194 | RE_PD_LAP_DISALLOWED | 39 | "Player fastest lap disallowed (exiting under red)" |
| CI-195 | RE_PD_SG_CUT | 151 | "Player stop/go for cutting track" |
| CI-196 | RE_PD_SG_DD | 152 | "Player stop/go for dangerous driving" |
| CI-197 | RE_PD_SG_SPEED | 153 | "Player stop/go for speeding in pits" |
| CI-198 | RE_PD_WIN_EUR | 160 | "Player wins European GP" |
| CI-199 | RE_OD_EXIT_RED | 36 | "Other driver under investigation (exiting under red)" |
| CI-200 | RE_OD_LAP_DISALLOWED | 37 | "Other driver fastest lap disallowed (exiting under red)" |
| CI-201 | RE_OD_SG_DD | 251 | "Other driver stop/go for dangerous driving" |
| CI-202 | RE_OD_FASTEST_M | 290 | "Other driver fastest on track — Monsoon tires" |
| CI-203 | Misc_14 | 198 | "AI Driver takes race lead" |
| CI-204 | Misc_16 | 264 | "Player switch camera to AI Driver" |
| CI-205 | Scar01 | 423 | "Safety car entering track" |
| CI-206 | Offroad02 | n/a | Player off track alt (orphaned — see ORPHANED section) |
| CI-207 | 2001_224 | 284 | "Player drives the wrong way" |
| CI-208 | 2001_312 | 287 | "Fastest AI on Wets" |
| CI-209 | 2001_311 | 285 | "Fastest AI on Inters" |  *(duplicate of CI-030, same eid)*
| CI-210 | AILappin02 | 123 | "Player under blue flag — 1st warning" |
| CI-211 | AILappin03 | 124 | "Player under blue flag — 2nd warning" |
| CI-212 | AILappin04 | 125 | "Player under blue flag — 3rd warning" |

---

## EASY — Unblock cooldown, then inject

**Technique:** Write `float 1.0` (0x3F800000) at `EVENT_TABLE (0x5BF9A0) + eid*0x14 + 0x0C`, then inject eid.
You can also just inject eid — the tool bypasses cooldown for live testing. But permanent fix requires the cooldown write.

### Player Gearbox Failure — Lost Gears (6 clips)

| # | Clip | EID | Engineer says | Cooldown offset |
|---|------|----:|---------------|-----------------|
| BL-01 | MechProb20 | 80 | "Player lost 1st gear" | `EVENT_TABLE + 80*0x14 + 0x0C` = `0x5BFA00 + 0x0C` |
| BL-02 | MechProb21 | 81 | "Player lost 2nd gear" | `... + 81*0x14 + 0x0C` |
| BL-03 | MechProb22 | 82 | "Player lost 3rd gear" | `... + 82*0x14 + 0x0C` |
| BL-04 | MechProb23 | 83 | "Player lost 4th gear" | `... + 83*0x14 + 0x0C` |
| BL-05 | MechProb24 | 84 | "Player lost 5th gear" | `... + 84*0x14 + 0x0C` |
| BL-06 | MechProb25 | 85 | "Player lost 6th gear" | `... + 85*0x14 + 0x0C` |

### Player Gearbox Failure — Stuck Gears (8 clips)

> **Corrected 2026-07-27 from `data/progress.json`.** The clip names in this block
> were previously shifted one row up, because `MechProb26` ("Lost 7th gear") was
> missing from the lost-gear block above. The **eids were always right**; only the
> clip names and gear numbers were wrong. `MechProb32`/`33` are 6th/7th gear, not
> the "variant 7/8" guessed earlier.

| # | Clip | EID | Engineer says | Cooldown offset |
|---|------|----:|---------------|-----------------|
| BL-06b | MechProb26 | 86 | "Player lost 7th gear" *(belongs to the lost-gear block)* | `... + 86*0x14 + 0x0C` |
| BL-07 | MechProb27 | 87 | "Player stuck in 1st gear" | `... + 87*0x14 + 0x0C` |
| BL-08 | MechProb28 | 88 | "Player stuck in 2nd gear" | `... + 88*0x14 + 0x0C` |
| BL-09 | MechProb29 | 89 | "Player stuck in 3rd gear" | `... + 89*0x14 + 0x0C` |
| BL-10 | MechProb30 | 90 | "Player stuck in 4th gear" | `... + 90*0x14 + 0x0C` |
| BL-11 | MechProb31 | 91 | "Player stuck in 5th gear" | `... + 91*0x14 + 0x0C` |
| BL-12 | MechProb32 | 92 | "Player stuck in 6th gear" | `... + 92*0x14 + 0x0C` |
| BL-13 | MechProb33 | 93 | "Player stuck in 7th gear" | `... + 93*0x14 + 0x0C` |

### Player Wing/Wheel Loss (6 clips — part detach commentary)

| # | Clip | EID | Engineer says | Cooldown offset |
|---|------|----:|---------------|-----------------|
| BL-15 | Hit01 | 108 | "Player lost front wing" | `... + 108*0x14 + 0x0C` |
| BL-16 | Hit02 | 111 | "Player lost rear wing" | `... + 111*0x14 + 0x0C` |
| BL-17 | Hit03 | 106 | "Player lost wheel — Front Left" | `... + 106*0x14 + 0x0C` |
| BL-18 | Hit04 | 107 | "Player lost wheel — Front Right" | `... + 107*0x14 + 0x0C` |
| BL-19 | Hit05 | 109 | "Player lost wheel — Rear Left" | `... + 109*0x14 + 0x0C` |
| BL-20 | Hit06 | 110 | "Player lost wheel — Rear Right" | `... + 110*0x14 + 0x0C` |

### Player Retirement + Stall (4 clips)

| # | Clip | EID | Engineer says | Cooldown offset |
|---|------|----:|---------------|-----------------|
| BL-21 | Retire01 | 118 | "Player engine stall" | `... + 118*0x14 + 0x0C` |
| BL-22 | Retire02 | 117 | "Player generic retirement (variant 1)" | `... + 117*0x14 + 0x0C` |
| BL-23 | Retire04 | 119 | "Player blown engine" | `... + 119*0x14 + 0x0C` |
| BL-24 | RE_PD_DQ | 149 | "Player DSQ for failing to serve stop/go" | `... + 149*0x14 + 0x0C` |

### Player Engine Stall (T-Car) (2 clips)

| # | Clip | EID | Engineer says | Cooldown offset |
|---|------|----:|---------------|-----------------|
| BL-25 | RE_PD_STALLED | 112 | "Player engine stalls, team prepares T-car" | `... + 112*0x14 + 0x0C` |
| BL-26 | RE_PD_STALLED_T | 113 | "Player engine stalls on T-car, session ends" | `... + 113*0x14 + 0x0C` |

---

## MEDIUM — Carrier event injection needed

**Technique:** These 23 SDT clips have zero events pointing to them. To play them:
1. Pick a carrier event (dead event with score=-1, like eid 429-446)
2. Write the orphaned clip's SDT index into `word_5BEBB0[score]` (PWS lookup table)
3. Fill `EVENT_TABLE + carrier_eid*0x14` with priority, score, cooldown
4. Inject carrier_eid via commentary_inject.py
5. Restore original values

Reference: `tools/sc_audio_test.py` (proven carrier injection)

### Safety Car (1 clip orphaned — Scar04)

| # | Clip | SDT Index | Engineer says | Alive sibling |
|---|------|----------:|---------------|---------------|
| OR-01 | Scar04 | 141 | "Safety car in pit lane" | Scar05 (eid 428, SDT 137) |

### AI Commentary (7 clips)

| # | Clip | SDT Index | Engineer says | Alive sibling |
|---|------|----------:|---------------|---------------|
| OR-02 | AIPit05 | 189 | "AI Driver pits (alt)" | AIPit04 (eid 178) |
| OR-03 | AIPit07 | 191 | "AI Driver pits (alt 2)" | AIPit06 (eid 179) |
| OR-04 | AIRetire01 | 253 | "AI straight up retired" | AIRetire02 (eid 237) |
| OR-05 | AIWin03 | 277 | "AI finishes at X position (alt)" | AIWin02 (eid 260) |
| OR-06 | AIIncid02 | 242 | "Went off track in specific sector" | AIIncid03 (eid 228) |
| OR-07 | AIIncid05 | 247 | "Spun off generic" | AIIncid04 (eid 232) |
| OR-08 | MISC12 | 211 | "AI Driver takes race lead (alt)" | MISC14 (eid 198) |

### Player Commentary (5 clips)

| # | Clip | SDT Index | Engineer says | Alive sibling |
|---|------|----------:|---------------|---------------|
| OR-09 | Hit09 | 115 | "Player hits AI Driver (alt)" | Hit08 (eid 115), Hit10 (eid 114) |
| OR-10 | Offroad02 | 117 | "Player goes off track (alt)" | Offroad01 (eid 116) |
| OR-11 | Overtake02 | 123 | "Player overtakes AI Driver (alt 1)" | Overtake01 (eid 120) |
| OR-12 | Overtake03 | 124 | "Player overtakes AI Driver (alt 2)" | Overtake01 (eid 120) |
| OR-13 | Retire03 | 119 | "Player ran out of fuel" | Retire01/02/04 (BLOCKED) |

### Weather / Track (3 clips)

| # | Clip | SDT Index | Engineer says | Alive sibling |
|---|------|----------:|---------------|---------------|
| OR-14 | 2001_219 | 302 | "Rain starts, won't get much worse (drizzle)" | 2001_220 (eid 283) |
| OR-15 | 2001_223 | 304 | "Player drives the wrong way (alt 1)" | 2001_224 (eid 284) |
| OR-16 | 2001_225 | 305 | "Player drives the wrong way (alt 2)" | 2001_224 (eid 284) |

### Qualifying / Race (3 clips)

| # | Clip | SDT Index | Engineer says | Alive sibling |
|---|------|----------:|---------------|---------------|
| OR-17 | 2001_09 | 32 | "AI Driver is on Pole Position (alt)" | 2001_08 (eid 33) |
| OR-18 | 2001_110 | 177 | "Player wins race" | Win01-19 (multiple eids) |
| OR-19 | 2001_161 | 290 | "X laps to go (alt)" | 2001_160 (eid 272) |

### Porsche — DLC/Track-Specific (4 clips)

| # | Clip | SDT Index | Engineer says | Notes |
|---|------|----------:|---------------|-------|
| OR-20 | RE_PORSCHE_23 | 312 | Porsche-specific commentary | No event mapping |
| OR-21 | RE_PORSCHE_24 | 313 | Porsche-specific commentary | No event mapping |
| OR-22 | RE_PORSCHE_25 | 314 | Porsche-specific commentary | No event mapping |
| OR-23 | RE_PORSCHE_03 | 315 | Porsche-specific commentary | No event mapping |

---

## HARD — Needs tick-hook DLL (new emitter logic)

These clips are ALIVE (valid event entry) or ORPHANED but NEVER fire because the
code that SHOULD trigger them doesn't exist. You must build a DLL that polls game
state and fires events at the right moments.

Reference DLL skeleton: `mods/grid_reorder/gr28.c` (tick hook + VEH macro + inject_commentary).

### H1 — Weather Commentary Emitter (6 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| 2001_218 | 282 | Track wetness crosses Overcast threshold |
| 2001_220 | 283 | Rain intensity enters Shower range |
| 2001_221 | 280 | Rain intensity enters Monsoon range |
| 2001_222 | 281 | Rain intensity drops below threshold |
| 2001_310 | 286 | Fastest AI lap set on Slicks during rain conditions |
| 2001_311 | 285 | Fastest AI lap set on Inters during rain conditions |
| 2001_312 | 287 | Fastest AI lap set on Wets during rain conditions |

DLL polls: track wetness float, rain intensity, per-driver tire type, fastest lap holder.

### H2 — Session Phase Emitter (4 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| 2001_163 | 274 | Race distance >= 25%, < 25.5% |
| 2001_164 | 271 | Race distance >= 50%, < 50.5% |
| 2001_165 | 270 | Race distance >= 75%, < 75.5% |
| 2001_167 | 273 | Current lap == total laps - 1, player crosses start/finish |

DLL polls: race distance percentage, current lap, total laps.

### H3 — Pit Stop Procedure Emitter (16 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| 2001_68-71 | 62,64,67,69 | Pit stop state machine enters "tire change done" per corner |
| 2001_72-75 | 63,65,68,70 | Pit stop state machine enters "slow tire" per corner |
| 2001_76-77 | 61,66 | Wheel gun failure/jam state |
| 2001_78 | 57 | Refuel complete state |
| 2001_79 | 56 | Fuel hose issue state |
| 2001_80 | 54 | Engine stall during pit state |
| Pit06 | 51 | Player enters pit limiter zone |
| Pit12 | 43 | Player exits pit limiter zone |
| Pit13 | 48 | Stop-go penalty countdown active |

DLL polls: PitSelectionStateMachine state, limiter flag, stop-go timer.

### H4 — Qualifying Run-Counter Emitter (10 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| 2001_13-15 | 32,30,31 | AI driver detected on out/flying/in lap |
| 2001_17-20 | 23-26 | AI driver starts qualifying run N (N=1-4) |
| 2001_22 | 27 | Weather forecast says rain soon, session still dry |
| 2001_24 | 28 | Weather forecast says rain ending soon |
| 2001_25 | 29 | Weather forecast says rain continuing |

DLL polls: per-driver qualifying run state, weather forecast data.

### H5 — Per-Tire Blistering Emitter (8 clips)

| Clip | EID | Trigger condition |
|------|----:|---------------|
| RE_PD_FL_BAD_BL | 94 | FL tire wear >= severe blistering threshold |
| RE_PD_FL_BLIST | 95 | FL tire wear >= light blistering threshold |
| RE_PD_FR_BAD_BL | 96 | FR tire wear >= severe |
| RE_PD_FR_BLIST | 97 | FR tire wear >= light |
| RE_PD_RL_BAD_BL | 98 | RL tire wear >= severe |
| RE_PD_RL_BLIST | 99 | RL tire wear >= light |
| RE_PD_RR_BAD_BL | 100 | RR tire wear >= severe |
| RE_PD_RR_BLIST | 101 | RR tire wear >= light |

DLL polls: per-corner tire wear float per frame.

### H6 — Player Mechanical Failure Emitter (22 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| MechProb06-09 | 78-79,102-103 | Puncture detected per corner (unblocked, emitter missing) |
| MechProb14 | 76 | Electronics damage flag set |
| MechProb15 | 77 | Engine temperature > threshold |
| MechProb16 | 75 | Suspension damage flag set |
| MechProb18 | 74 | Brake temperature > threshold |
| MechProb20-33 | 80-93 | Gear failure detected (BLOCKED — unblock first) |
| MechWarn0 | 130 | Meatball flag active (no code, audio only) |
| Hit01-06 | 106-111 | Part detach detected (BLOCKED — unblock first) |
| Retire01-04 | 117-119 | Player retirement flag (BLOCKED — unblock first) |
| RE_PD_STALLED | 112-113 | Engine stall + T-car stall (BLOCKED — unblock first) |
| RE_PD_DQ | 149 | Player DSQ (BLOCKED — unblock first) |

DLL polls: per-vehicle damage/detach/retirement flags. Note: BLOCKED clips need
their cooldown written to `1.0` before the emitter's event injection will work.

### H7 — AI Gearbox Failure Emitter (14 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| AIMechProb lost 1-6 | 201-206 | AI driver loses gear X |
| AIMechProb stuck 1-6 | 207-208 + 212-217 | AI driver stuck in gear X (actually mapped to same lost clips) |
| RE_OD_LOST_7 | 209 | AI driver loses 7th gear |
| RE_OD_STUCK_7 | 211 | AI driver stuck in 7th gear |
| RE_OD_PUNCTURE | 210 | AI driver has puncture |

DLL polls: AI vehicle damage/enum state per driver.

### H8 — AI Incident Chain Extension (4 clips)

| Clip | EID or SDT | Trigger condition |
|------|-----------|-------------------|
| AIIncid02 | ORPHANED SDT 242 | AI went off in specific sector |
| AIIncid05 | ORPHANED SDT 247 | AI spun off generic |
| AIIncid03 | 228 | AI went off generic (ALIVE but emitter covered by switch) |
| AIIncid04 | 232 | AI spun in specific sector (ALIVE but emitter covered by switch) |

Fix: extend switch in `sub_41A8C0` to cover all 11 incident types, or build DLL
that polls AI off-track state and fires the right eid. Two orphaned clips need
carrier wiring first.

### H9 — AI Penalty Commentary Emitter (5 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| AIPENALTY01-05 | 256,254,255,252,242 | AI driver gets penalty (false start, speeding, etc.) |

Fix: `sub_41A720` only fires penalty commentary for player. Extend or build DLL
that polls AI penalty state.

### H10 — Blue Flag Approach Chain Emitter (5 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| AIBlue01 | 258 | Player within X meters behind lapped AI |
| AIBlue02 | 257 | Player within Y meters behind lapped AI |
| AIBlue03 | 259 | Player passes lapped AI |
| AILappin01 | 122 | AI within X meters behind player (blue flag) |
| AILappin05 | 121 | AI passes player |

DLL polls: per-driver lap count, distance to car ahead/behind, trigger at ranges.

### H11 — Championship Commentary Emitter (6 clips)

| Clip | EID | Trigger condition |
|------|----:|-------------------|
| 2001_33 | 40 | Player position >= threshold to secure WDC |
| 2001_138 | 200 | AI driver position >= threshold to secure WDC |
| 2001_141 | 199 | Team WCC position threshold met |
| 2001_124 | 159 | Player secures WDC with rounds remaining |
| 2001_308 | 289 | AI driver within chase range for championship |
| 2001_309 | 288 | AI team issues driver orders (championship context) |

DLL polls: championship points per driver, rounds remaining, current race position.

---

## BONUS: Bug Fixes (Emitter Fixes category)

These are existing emitters with wrong logic. No new DLL needed — patch the guard condition.

| # | Bug | EID | Problem | Fix |
|---|-----|----:|---------|-----|
| BUG-01 | Qual02/03 fires on slow sectors | 4,5 | No time comparison — fires regardless of sector time | Patch to compare sector time vs. provisional pole |
| BUG-02 | AIPit10 "3-stop" fires on 1-stoppers | 182 | Strategy counter wrong — triggers incorrectly | Patch condition in `sub_41A8C0` strategy detection |
| BUG-03 | AILappin02-04 coverage partial | 123-125 | Blue flag chain only fires in rare conditions | Extend trigger range in blue-flag checker |

---

## Quick Reference: Injection Commands

```bash
# Single event test (game must be running, on track, commentary enabled)
python commentary_inject.py <eid>

# Unblock an event (via Cheat Engine or custom DLL)
# Write float 1.0 at EVENT_TABLE + eid*0x14 + 0x0C

# Example: unblock + play "Player lost front wing" (Hit01, eid 108)
# 1. Write 0x3F800000 (float 1.0) to 0x5BF9A0 + 108*0x14 + 0x0C = 0x5BFA00 + 0xD8 + 0x0C = 0x5BFAE4
# 2. python commentary_inject.py 108
```

## Quick Reference: Config Flips

Game closed, then text-substitute in the .plr:

```python
import re, pathlib
plr = pathlib.Path(r"%USERPROFILE%\Desktop\F1 Challenge V10\...\<name>.PLR")
t = plr.read_text(encoding="latin1")
plr.with_suffix(".PLR.bak").write_text(t, encoding="latin1")   # back up first

t = t.replace('GPRIX Formation Lap="0"', 'GPRIX Formation Lap="1"')
t = t.replace('Run Practice2="0"', 'Run Practice2="1"')
t = re.sub(r'Night Lighting="[\d.]+"', 'Night Lighting="0.50000"', t)

plr.write_text(t, encoding="latin1")
```

## Stats Summary

| Difficulty | Category | Count |
|-----------|----------|-------|
| TRIVIAL | Config Flips | 5 |
| TRIVIAL | Commentary Injection (ALIVE) | ~210 eids |
| EASY | Commentary Injection (BLOCKED) | 26 clips |
| MEDIUM | Commentary Injection (ORPHANED) | 23 clips |
| HARD | Tick-Hook DLL (new emitters) | 11 emitter groups |
| — | Bug Fixes | 3 |
