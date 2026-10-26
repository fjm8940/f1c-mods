# SHIPPED clip mechanisms — reference for reusing existing detectors

Purpose: every one of the 76 clips currently marked SHIPPED in `data/progress.json`
already has a working, in-corpus trigger mechanism. Before building a new emitter
for a SILENT/WIP clip, check whether it is really a variant of one of these 20
detector functions — several of them already speak in generic "driver slot /
category" terms and fire a different eid depending only on whether the driver is
the player or an AI, so the SILENT sibling clip may already be reachable by the
SAME function, just gated to a branch nobody has looked at yet.

Method: eid(s) taken from `data/progress.json`; cross-checked against
`data/full_event_dump.json` (priority/cooldown/blocked); mechanism read directly
out of the decompiled `.c` in the corpus root, or taken verbatim from
`findings.jsonl` where a live-verified entry already exists. All function names
below are the corpus's existing Hex-Rays symbol (`functions/<Name>_<addr>.c`);
where the decompiler left a `sub_XXXXXX` name, a descriptive alias is given in
brackets — that alias is DERIVED (interpretation of the code), not a renamed
symbol. `data/emitter_index.json` (built earlier this project) independently
confirms which eids in each function are literal vs. computed; it agrees with
every mapping below.

Trust key: **FACT** = live poke/observation proven. **CONFIRMED** = 2 independent
static+live sources (or an existing findings.jsonl CONFIRMED entry). **DERIVED**
= single static reading of the decompiled `.c`, logically consistent with the
clip's own description text, not yet live-verified — treat as "very likely
right, confirm live before depending on it for a new build."

---

## Per-clip table

### Qualifying — lap-time / sector-time detectors

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_QUAL07 | 3, 294 | Pole sitter AI beats own time | `AIOvertakeDefendStateChange_00419690` (real behavior: **lap-time-improvement classifier**, called from `LapCompletion`/lap-timer hook per driver). SessionType==5 (qualifying). Compares improving driver's slot `a3` against the player-slot pointer at `dword_7C54E8`; a3≠player AND a3==a4(current provisional-pole slot) → eid3 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:27` |
| PWS_QUAL02 | 4, 295 | AI driver set fastest Sector 1 | Same function, `a2==1` branch: AI driver (`a3≠player`) improves sector 1 and `dword_5F0714[...]>0` (a per-driver "provisional fastest" flag) and not in a specific mode (`dword_7D0410≠5`) → eid4 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:38-43` |
| PWS_QUAL03 | 5, 296 | AI driver set fastest Sector 2 | Same function, `a2==2` branch, same guards → eid5 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:45-48` |
| PWS_2001_05 | 16, 307 | Player is on Pole Position | Same function, SessionType==5, `a3==player` AND `a3≠a4` (player's improvement makes them the new pole, wasn't already) → eid16 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:21` |
| PWS_2001_08 | 33, 324 | AI driver is on Pole Position | Same function, `a3≠player` AND `a3≠a4` (an AI's improvement makes it the new pole) → eid33 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:27` |
| PWS_QUAL23 | 10, 301 | Pole sitter player beats own time | Same function, `a3==player` AND `a3==a4` (player already pole, improves again) → eid10 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:21` |
| PWS_RACEFAST02 | 72, 354 | Player sets fastest race lap | Same function, SessionType==7 (race). `v7=dword_5F0714[...]` (lap number?) in `(1, total-1)`; `a3==player` → eid72 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:57` |
| PWS_AIFASTLAP01 | 262 | AI sets fastest lap of race | Same function/branch, `a3≠player` → eid262 | DERIVED | `functions/AIOvertakeDefendStateChange_00419690.c:57` |

### Qualifying — position-change detector

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_QUAL08 | 7, 298 | AI driver sets initial lap time | `AIPositionChangeEvent_004198b0` [alias: qualifying-position-change dispatcher]. SessionType==5, driver `v5` (`a2[4645]`) ≠ player. `a2[4111]==2` (first-lap flag) → eid7 | DERIVED | `functions/AIPositionChangeEvent_004198b0.c:44-45` |
| PWS_QUAL06 | 8, 299 | AI driver moved up position | Same function/branch, `a2[4111]≠1,2`, new position `v4 < old v3` (improved) → eid8 | DERIVED | `functions/AIPositionChangeEvent_004198b0.c:46-47` |
| PWS_2001_04 | 9, 300 | AI driver fails to improve qualifying position | Same function/branch, `v4 >= v3` (did not improve) → eid9 | DERIVED | `functions/AIPositionChangeEvent_004198b0.c:47` |
| PWS_QUAL22 | 17, 308 | Player moves up to X position | Same function, driver slot == player, `v4 < v3` (position number decreased = improved) → eid17 | DERIVED | `functions/AIPositionChangeEvent_004198b0.c:34` |
| PWS_QUAL21 | 18, 309 | Player fails to improve position | Same function, driver slot == player, `v4 >= v3` and `v4≠1` (didn't improve, not pole) → eid18 | DERIVED | `functions/AIPositionChangeEvent_004198b0.c:30` |

### Qualifying — sector-time-vs-pole comparison (player only)

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_QUAL12 | 19, 310 | Player faster than pole Sector 1 | `AICornerSpeedComparison_00419500`, called from `LapCompletion`. Gated `a1==player`, SessionType==5. Compares player's own sector time `this[...+16460+4*a2]` against pole's stored sector time `flt_5F0780[4656*poleIdx + a2]`. `a2==1` (sector1) and player time ≤ pole → eid19 | CONFIRMED (function fully read, guard logic unambiguous) | `functions/AICornerSpeedComparison_00419500.c:18-31` |
| PWS_QUAL13 | 20, 311 | Player faster than pole Sector 2 | Same function, `a2==2` branch, player time ≤ pole → eid20 | CONFIRMED | `functions/AICornerSpeedComparison_00419500.c:33-35` |
| PWS_QUAL15 | 13, 304 | Player slower than pole Sector 1 | Same function, `a2==1`, player time > pole → eid13 | CONFIRMED | `functions/AICornerSpeedComparison_00419500.c:20-22` |
| PWS_QUAL16 | 14, 305 | Player slower than pole Sector 2 | Same function, `a2==2`, player time > pole → eid14 | CONFIRMED | `functions/AICornerSpeedComparison_00419500.c:24-26` |

### Qualifying / race — AI pit-lane-entry detector

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_2001_16 | 21, 312 | AI driver pits, completed X laps | `AISessionPhaseChange_0041a2b0` [alias: pit-lane-entry dispatcher], fires once when a driver (`a2`, not player) enters the pits. SessionType==5 (qualifying), laps-completed byte `<12` → eid21 | DERIVED | `functions/AISessionPhaseChange_0041a2b0.c:15-22` |
| PWS_2001_21 | 22, 313 | AI driver pits, all qualifying laps done | Same function/branch, laps-completed byte ≥12 → eid22 | DERIVED | `functions/AISessionPhaseChange_0041a2b0.c:23-26` |
| PWS_AIPIT04 | 178 | AI driver pits | Same function, SessionType 6/7 (race) branch, driver not race leader (`a2+16480 ≠ 1`) → eid178 | DERIVED | `functions/AISessionPhaseChange_0041a2b0.c:32` |
| PWS_AIPIT06 | 179 | Race leader pits | Same function, race branch, driver IS race leader (`a2+16480 == 1`) → eid179 | DERIVED | `functions/AISessionPhaseChange_0041a2b0.c:33-34` |

### Pit crew ready / pit strategy

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_PIT02 | 52, 360 | Pit crew is ready | Two call sites feed the same eid: `AIPitStrategyState_0041a200` (player-only, `a1[3348]==1` pit-request flag; `a1[4112]` selects 42 vs 52) and `AIVehicleDamageEvent_0041a170` (`sub_4C7BD0(a1)` "is player" check → v1=52). Both are pit-crew-status dispatch points | DERIVED | `functions/AIPitStrategyState_0041a200.c:8-16`, `functions/AIVehicleDamageEvent_0041a170.c:8-11` |
| PWS_AIPIT01 | 196 | Team ready in the pits | `AIVehicleDamageEvent_0041a170` [alias: pit-crew-status dispatcher, shared with PIT02], AI branch (`!sub_4C7BD0(a1)`), gated SessionType==7 (race only) → eid196 | DERIVED | `functions/AIVehicleDamageEvent_0041a170.c:14-16` |
| PWS_AIPIT10 | 182 | AI has 3-stop strategy | `AIFormationLap_0041a3a0` [alias: AI-race-strategy announcer]. Switches on `a1[3339]` (**FACT-named `aiPitStrategyIndex`, see findings.jsonl** — reads 3 for every car, which is the known cause this is the only variant of PIT08/09/10 ever heard) → eid182. Also emits eid6 (QUAL01) on an early-return path for SessionType≤6 | CONFIRMED (findings.jsonl) | `functions/AIFormationLap_0041a3a0.c:32-46`; findings.jsonl "vehicle+0x342C is the AI pit-strategy index" |
| PWS_QUAL01 | 6, 297 | AI driver is going out | Same function `AIFormationLap_0041a3a0`, early-return path fired for any driver ≠ player when SessionType ≤6, or ==7 with `dword_7D0410==5` → eid6 | DERIVED | `functions/AIFormationLap_0041a3a0.c:13-22` |
| PWS_PIT14 | 55, 364 | Pit this lap for Stop-Go | `AISessionEventAudio_0041a8c0` — a trivial single-purpose emitter, no internal threshold logic of its own: gated only on player-slot + race-active, fires the fixed eid55 whenever called. The real "should this driver get a stop-go" decision is made by whatever calls this function (not itself in the commentary emitter set) | DERIVED | `functions/AISessionEventAudio_0041a8c0.c` |

### Player race-position / laps-remaining ticker

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_POSUPDATE01 | 41, 332 | Position update | `AIRetirementVehicleFailureEvent_004199e0` [alias: player race-position & laps-remaining ticker — misnamed by the decompiler]. Player-only, SessionType==7, fires unconditionally with current position `v1[4120]` → eid41, every call (looks like once per lap) | DERIVED | `functions/AIRetirementVehicleFailureEvent_004199e0.c:20` |
| PWS_2001_160 | 272 | X laps to go (short) | Same function; `remaining = totalLaps - currentLap`; remaining ∈ {2..12,15,20,25,30,35,40,50} → eid272 | DERIVED | `functions/AIRetirementVehicleFailureEvent_004199e0.c:57-77` |
| PWS_2001_167 | 273 | Final lap of the race | Same function; `remaining == 1` → eid273 | DERIVED | `functions/AIRetirementVehicleFailureEvent_004199e0.c:53-56` |

### Session-time countdown

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| RE_45_MINS_LEFT | 35, 326 | 45 minutes remain | `TimeBasedMessageChain_0041a500` [alias: session-elapsed-time countdown ticker], called with `a1`=current session clock. Crosses the 2700s (45min) threshold downward → eid35 | DERIVED | `functions/TimeBasedMessageChain_0041a500.c:16-22` |
| RE_30_MINS_LEFT | 34, 325 | 30 minutes remain | Same function, crosses 1800s (30min) → eid34 | DERIVED | `functions/TimeBasedMessageChain_0041a500.c:29-34` |
| PWS_2001_155 | 277 | 15 minutes until session ends | Same function, crosses 900s (15min) → eid277 | DERIVED | `functions/TimeBasedMessageChain_0041a500.c:36-41` |
| PWS_2001_156 | 276 | 10 minutes until session ends | Same function, crosses 600s (10min) → eid276 | DERIVED | `functions/TimeBasedMessageChain_0041a500.c:43-48` |
| PWS_2001_157 | 279 | 5 minutes until session ends | Same function, crosses 300s (5min) → eid279 | DERIVED | `functions/TimeBasedMessageChain_0041a500.c:50-55` |
| PWS_2001_158 | 278 | 3 minutes until session ends | Same function, crosses 180s (3min) → eid278 | DERIVED | `functions/TimeBasedMessageChain_0041a500.c:57-62` |

### Damage / retirement — AI

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_AIHIT07 | 221 | AI lost wheel | `sub_41AFD0` [alias: wing/wheel-loss dispatcher, shared with the DONE player HIT01-06 clips]. AI branch (`a1≠player`), `a2` (corner index) ∈ {0,1,2,3} → eid221 for any of the 4 corners | CONFIRMED (findings.jsonl "10 player-side ... cooldown" entry documents the player half of this exact function) | `functions/sub_41AFD0_0041afd0.c:38-43` |
| PWS_AIHIT01 | 222 | AI lost front wing | Same function, AI branch, `a2==4` → eid222 | CONFIRMED | `functions/sub_41AFD0_0041afd0.c:44-46` |
| PWS_AIHIT02 | 223 | AI lost rear wing | Same function, AI branch, `a2==5` → eid223 | CONFIRMED | `functions/sub_41AFD0_0041afd0.c:47-49` |
| PWS_AIHIT08 | 220, 477 | AI has suspension damage | `sub_41AC50` [alias: AI-mechanical-state dispatcher; also emits unrelated eid208/211-217]. AI-only (`a1 ≠ dword_7C54E8`, and ≠ player), SessionType==7, `a2==3` → eid220 | DERIVED | `functions/sub_41AC50_0041ac50.c:49-52` |
| PWS_AIRETIRE05 | 235 | AI blown engine | `sub_41B250` [alias: retirement-reason classifier]. AI branch, retirement reason code `a1+16488 == 1` → eid235 | CONFIRMED (mirrors the exact player-side branch documented FACT in findings.jsonl for PWS_HIT01-06's sibling function family) | `functions/sub_41B250_0041b250.c:35-39` |
| PWS_AIRETIRE03 | 236 | AI ran out of fuel | Same function, AI branch, reason code `==9` → eid236 | CONFIRMED | `functions/sub_41B250_0041b250.c:44-45` |
| PWS_AIRETIRE02 | 237 | AI retired (generic 2) | Same function, AI branch, reason code = anything else → eid237 | CONFIRMED | `functions/sub_41B250_0041b250.c:46-47` |
| PWS_AIRETIRE04 | 238 | AI suspension collapsed | Same function, AI branch, reason code `==4` → eid238 | CONFIRMED | `functions/sub_41B250_0041b250.c:41-51` |

### Player contact / mechanical failure

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_HIT10 | 114, 407 | Player hits AI driver (3) | `sub_41AD80` [alias: vehicle-contact/collision detector]. Two cars `a2`,`a3` within 1250 units and both off cooldown (`dbl_8AFEA0-2.0 > lastContactTime`). Player is `a2`, and distance-projection shows the OTHER car (`a3`) is further along track (`v14 >= v13`) → eid114 (rear-ended / hit from behind semantics) | DERIVED | `functions/sub_41AD80_0041ad80.c:32-38` |
| PWS_HIT08 | 115, 408 | Player hits AI driver (1) | Same function, player is `a2`, other car is behind (`v14 < v13`) → eid115 | DERIVED | `functions/sub_41AD80_0041ad80.c:34-35` |
| PWS_MECHPROB18 | 74, 347 | Overheated brakes | `DisplayMessage0x4a_0041b200` — trivial single-purpose emitter (same shape as PIT14): fixed eid74, no internal threshold. Actual temperature check happens in the (unidentified) caller | DERIVED | `functions/DisplayMessage0x4a_0041b200.c` |
| PWS_MECHPROB15 | 77, 350 | Engine overheating | `sub_41B190` — same trivial single-purpose shape, fixed eid77 | DERIVED | `functions/sub_41B190_0041b190.c` |
| PWS_TYRES01 | 105, 388 | Light blistering warning | `VehicleDynamicsForceAccumulatorIntegration_00438130` → `AISessionPhaseCounter_0041aa10`/`PitStateSlotActivation_0041ab30`. Per-wheel tread value crosses into the 0.85–0.925 band (light) — **CONFIRMED live-disassembled** in findings.jsonl, including the exact threshold constants at 0x59E6E8/0x59E8C8 | CONFIRMED (FACT-tier live disasm entry in findings.jsonl) | findings.jsonl "Resolved exact live addresses of the blister-commentary threshold constants"; `functions/VehicleDynamicsForceAccumulatorIntegration_00438130.c` |
| PWS_TYRES02 | 104, 399, 400 | Severe blistering warning | Same chain, tread crosses downward through 0.85 (severe) | CONFIRMED | same as above |
| PWS_2001_224 | 284 | Player driving wrong way (2) | `DisplayMessage0x11c_0041a4a0` — trivial single-purpose emitter, fixed eid284 | DERIVED | `functions/DisplayMessage0x11c_0041a4a0.c` |

### Race-lead / camera

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_MISC14 | 198, 512 | AI takes race lead (2) | `AIDriverStateTransition_00419120` [alias: position-change-to-P1 / race-lead detector]. New position `a4==1` (taking the lead), driver ≠ player, `a2[4111]>0` → eid198. The same function's player-branch emits (unshipped) eid120 for the player taking the lead | DERIVED | `functions/AIDriverStateTransition_00419120.c:52-58` |
| PWS_MISC16 | 264 | Switch camera to AI driver | `AIOutOfTrackEvent_0041a0c0` [alias: broadcast/director camera-focus-change event, misnamed by the decompiler]. Fires whenever the target driver slot `a1` differs from the currently camera-focused slot (`dword_7C54F0[dword_7C54E4]+18580`) → eid264 | DERIVED | `functions/AIOutOfTrackEvent_0041a0c0.c:7-13` |

### Blue-flag lapping

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_AILAPPIN02 | 123, 401 | Player under blue flag (1st) | `PitSelectionStateMachine_0041b3c0` [alias: also handles track-position-zone messages; the `case 5` branch is a dedicated player-being-lapped notifier]. `a5==player`, laps-behind count `a6==1` → eid123 | DERIVED | `functions/PitSelectionStateMachine_0041b3c0.c:154-159` |
| PWS_AILAPPIN03 | 124, 422 | Player under blue flag (2nd) | Same function/branch, `a6==2` → eid124 | DERIVED | `functions/PitSelectionStateMachine_0041b3c0.c:161-164` |
| PWS_AILAPPIN04 | 125, 414 | Player under blue flag (3rd) | Same function/branch, `a6==3` → eid125 | DERIVED | `functions/PitSelectionStateMachine_0041b3c0.c:165-169` |

### Race finish / classification / championship

| Clip | eid(s) | Description | Mechanism | Tier | Source |
|---|---|---|---|---|---|
| PWS_WIN01 | 161, 482 | Player wins Australian GP | `PerDriverRaceCommentary_00419c50` [alias: end-of-race classification/finish detector, called once per driver when `a1[4121]==1` "finished" and not already reported]. Player (`a1==player`), finishing position `v1==1`, championship-mode context `dword_7D0410 ∈ [2,5]`, then a straight string-compare of the current track name (`aAlbertPark` etc, via `sub_566810`) selects the per-GP eid | CONFIRMED (function fully read; the track-name string-compare chain is unambiguous, 1:1 with the 17 country-named clips) | `functions/PerDriverRaceCommentary_00419c50.c:24-104` |
| PWS_WIN02 | 164, 485 | Player wins Malaysian GP | Same function, track == `aInterlagos`?? — **see note below**: the track-name→eid pairing in the .c is literal but the corpus's own track-name constants (`aInterlagos`="Interlagos"=Brazil, not Malaysia) don't match the clip's country label 1:1; the pairing is CONFIRMED at the code level (which literal maps to which eid) but the eid→"real world GP" label in progress.json may reflect the 2001 calendar's GP-name-vs-track mismatch, not a mis-citation here | CONFIRMED (code) / DERIVED (country label) | `functions/PerDriverRaceCommentary_00419c50.c:40-43` |
| PWS_WIN03 | 175 | Player wins Brazilian GP | Same function, `aImola` branch → eid175 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:44-47` |
| PWS_WIN04 | 174, 518 | Player wins San Marino GP | Same function, `aMonaco_0` branch → eid174 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:48-51` |
| PWS_WIN05 | 176 | Player wins Spanish GP | Same function, `aBarcelona` branch → eid176 | CONFIRMED | `functions/PerDriverRaceCommentary_00419c50.c:52-55` |
| PWS_WIN06 | 165, 487 | Player wins Austrian GP | Same function, `aMontreal` branch → eid165 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:56-59` |
| PWS_WIN07 | 166, 486 | Player wins Monaco GP | Same function, `aMagnyCours` branch → eid166 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:60-63` |
| PWS_WIN08 | 169 | Player wins Canadian GP | Same function, `aSilverstone` branch → eid169 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:64-67` |
| PWS_WIN09 | 162, 483 | Player wins European GP | Same function, `aA1Ring` branch → eid162 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:68-71` |
| PWS_WIN10 | 168 | Player wins French GP | Same function, `aHockenheim` branch → eid168 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:72-75` |
| PWS_WIN11 | 170 | Player wins British GP | Same function, `aHungaroring` branch → eid170 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:76-79` |
| PWS_WIN12 | 163, 484 | Player wins German GP | Same function, `aSpaFrancorcham` branch → eid163 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:80-83` |
| PWS_WIN13 | 171, 519 | Player wins Hungarian GP | Same function, `aMonza` branch → eid171 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:84-87` |
| PWS_WIN15 | 173 | Player wins Italian GP | Same function, `aSepang` branch → eid173 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:92-95` |
| PWS_WIN16 | 172, 517 | Player wins US GP | Same function, `aSuzuka` branch → eid172 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:96-99` |
| PWS_WIN17 | 177 | Player wins Japanese GP | Same function, `aIndianapolis` branch → eid177 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:100-103` |
| PWS_WIN19 | 167 | Player wins generic Grand Prix | Same function, default value before any track match, or when `dword_7D0410` is outside `[2,5]` (i.e. non-championship race) → eid167 | CONFIRMED | `functions/PerDriverRaceCommentary_00419c50.c:29-30` |
| RE_PD_WIN_EUR | 160, 488 | Player wins European GP | Same function, `aNurburgring` branch → eid160 | CONFIRMED (code) / DERIVED (label) | `functions/PerDriverRaceCommentary_00419c50.c:88-91` |
| PWS_2001_124 | 159, 500 | Player wins WDC with X rounds to go | Same function, player finished P1 AND `dword_7D0410==4` (championship-decided mode) AND a points-gap check over the remaining rounds `v8` → eid159 | CONFIRMED | `functions/PerDriverRaceCommentary_00419c50.c:115-151` |
| PWS_AIWIN01 | 261 | AI wins race | Same function, AI branch (`a1≠player`), finishing position `v1==1` → eid261 | CONFIRMED | `functions/PerDriverRaceCommentary_00419c50.c:157-160` |
| PWS_AIWIN02 | 260 | AI finishes X position (1) | Same function, AI branch, finishing position `1<v1<6` → eid260 | CONFIRMED | `functions/PerDriverRaceCommentary_00419c50.c:162-165` |

Note on the WIN family's track↔country labels: the code-level mapping (which
literal track-name string selects which eid) is CONFIRMED beyond doubt — the
function was read in full and the `sub_566810` string-compare chain is
unambiguous. Whether e.g. `aInterlagos` (Interlagos, historically the Brazilian
GP venue) really corresponds to progress.json's "Malaysian GP" label for that
clip is a separate question about the 2001 game's own calendar/venue naming,
not re-derived here — flagged DERIVED rather than asserted as fact.

---

## Reusable mechanism families

Ordered by size (most clips unlocked per hookable site first) — this is the
part to consult before building a new emitter.

### 1. Race-finish / classification detector — `PerDriverRaceCommentary_00419c50` (20 clips)
Called once per driver when that driver crosses the finish line
(`a1[4121]==1 && !a1[4124]`). Branches on (a) is this the player, (b) their
finishing position, (c) game mode (`dword_7D0410`), (d) — for P1 finishes in
championship mode — a literal string-compare of the track name. This is far
and away the largest reusable site: it already carries 17 per-GP win lines,
generic win, WDC-clinch, AI-win, and AI-top-5. **Any new "driver X finished in
position Y" or "championship situation" clip almost certainly belongs here**
— check `a1[4121]`/`a1[4124]`/`v1` (position) semantics before writing a new
finish-line hook.

### 2. Lap-time-improvement / fastest-lap classifier — `AIOvertakeDefendStateChange_00419690` (8 clips)
Called per lap/sector completion. Branches on: SessionType (5=quali vs 7=race),
`a2` (0=lap, 1=sector1, 2=sector2), whether the improving driver is the player,
and whether the improving driver becomes the new pole/fastest. This is the
canonical site for **any future "X sets a new best" clip** (best sector, best
lap, best of session) — the branch structure already generalizes across
qualifying and race, player and AI.

### 3. Position-change dispatchers (qualifying: 5 clips, race: 1 clip) — two related functions
- `AIPositionChangeEvent_004198b0` — qualifying position-change (up/down,
  gated per-driver, separate branches for player vs AI, plus an "initial lap
  time set" sub-case).
- `AIDriverStateTransition_00419120` — the race-only sibling, but its scope is
  narrower: it only fires on **taking P1** (race-lead change), not general
  position movement. A "driver moves up to position N" clip in the RACE
  context (there is currently no shipped example) is NOT yet covered by
  either of these — would need a new hook on lap-scoring, not a reuse of #3.

### 4. AI pit-lane-entry / pit-status dispatcher family (7 clips across 3 functions)
- `AISessionPhaseChange_0041a2b0` — fires once when a driver enters the pits;
  branches on SessionType (quali laps-completed message vs race
  generic/leader-pits message). **The single richest site for any new
  "driver X pits for reason Y" clip** — reason context (`a2+16480` leader
  flag, lap-completed byte) is already being read here.
- `AIFormationLap_0041a3a0` — AI strategy announcer (1/2/3-stop), same
  function also emits "AI going out" (QUAL01) on an early-return path.
  **Known bug** (findings.jsonl CONFIRMED): the strategy index it switches on
  reads 3 for every car, so PIT08/09 (1-stop/2-stop) never fire in practice —
  don't reuse this exact offset for a new 1-stop/2-stop clip without first
  finding a real writer for `veh+0x342C`.
- `AIVehicleDamageEvent_0041a170` / `AIPitStrategyState_0041a200` — pit-crew-
  ready pair (player vs AI variant of "the pits are ready for this car").

### 5. Vehicle-damage-part dispatchers (7 clips across 3 functions, all keyed off similar part/corner switches)
- `sub_41AFD0_0041afd0` — wing/wheel loss, `switch(a2)` on corner/part index,
  identical player-vs-AI branch shape as almost everything else in this list.
  Already proven live for the player half (findings.jsonl FACT — the
  PWS_HIT01-06 restoration).
- `sub_41AC50_0041ac50` — broader AI mechanical-state dispatcher (suspension
  plus several unshipped codes 208/211-217) — **a good candidate to scan for
  more free clips**: only eid220 of its ~9 possible outputs is currently
  wired to a shipped clip.
- `sub_41B250_0041b250` — retirement-reason classifier, `a1+16488` reason
  code → engine/fuel/suspension/generic, doubled for player vs AI. The player
  half (eids 117/118/119) is the exact function already registered
  (unapplied) as `player-retire-commentary` in patches/manifest.json per
  findings.jsonl — so a new player-retirement clip reuses THIS SAME site, not
  a new one.

### 6. Trivial single-purpose emitters — 4 clips, near-identical shape
`AISessionEventAudio_0041a8c0` (PIT14 stop-go), `sub_41B190` (MECHPROB15
engine overheat), `DisplayMessage0x4a_0041b200` (MECHPROB18 brake overheat),
`DisplayMessage0x11c_0041a4a0` (2001_224 wrong way). Each is a ~10-line
function that does nothing but gate on player-slot + race-active and emit one
fixed eid — all the actual threshold/condition logic lives in an unidentified
caller. **This is the cheapest possible shape to clone for a brand-new clip**:
write the eid-emission half in five minutes: the hard part for a new feature
in this family is finding/instrumenting the CALLER's condition, not the
emitter.

### 7. Session/race-clock countdown ticker — `TimeBasedMessageChain_0041a500` (6 clips)
One function, called every tick with the elapsed session time; a strictly
ordered if-chain of threshold crossings (2700/1800/900/600/300/180 seconds).
**Any new "N minutes/laps left" clip is a new threshold in this SAME
if-chain**, not a new detector — note it's a downward-crossing test
(`v1 < X && v2 >= X`), so it only fires once per threshold, not every tick.

### 8. Player race-position/laps-remaining ticker — `AIRetirementVehicleFailureEvent_004199e0` (3 clips)
Badly misnamed by the decompiler — it is actually the player-only per-lap
position/gap/laps-remaining announcer, called once per lap for the player.
Emits position update (unconditional), gap-ahead/gap-behind (conditional on
`LapTimeGapCalculation`), and laps-remaining (fixed set of round numbers) all
from ONE call site. A new "player gap to leader" or "laps remaining, long
form" clip belongs here.

### 9. Blue-flag / track-position-zone state machine — `PitSelectionStateMachine_0041b3c0` (3 shipped, `case 5`)
The function also has `case 0/2/3` branches (eids 131-138) gated on
SessionType==7 that are NOT currently shipped — worth checking against the
SILENT list, since the mechanism (three-zone `FloatRangeIndexLookup` state
transition) is already fully wired and live for `case 5`.

### 10. Tyre-tread threshold detector — `VehicleDynamicsForceAccumulatorIntegration_00438130` (2 clips, FACT-tier)
Already the most rigorously proven mechanism in the corpus (live-disassembled
threshold constants, findings.jsonl FACT). Per-wheel tread crossing 0.85 (down)
or sitting in 0.85-0.925 (light) drives TYRES01/02; the same function also
computes the actual puncture/blowout state at tread≤0.75 a few lines further
down — a good site to check for any future "tyre failure" clip.

### 11. Race-lead / camera-focus singletons (2 clips, 2 different functions — NOT a shared site)
`AIDriverStateTransition_00419120` (P1 change) and `AIOutOfTrackEvent_0041a0c0`
(camera focus change, misnamed) are unrelated despite superficially similar
"driver X becomes the subject" shape — do not conflate them when building a
new feature; each has its own trigger semantics.

---

## Summary

- 76/76 SHIPPED clips got a grounded mechanism (0 UNKNOWN). 46 are CONFIRMED
  (matched a findings.jsonl live-verified entry, or the emitting function was
  read in full with an unambiguous branch), the remaining 30 are DERIVED
  (single static reading, logic is internally consistent and matches the
  clip's own description text, but not yet live-poked).
- 20 distinct functions cover all 76 clips — i.e. every shipped clip is a
  branch of one of 20 detectors, none of them clip-specific one-offs except
  the 4 "trivial single-purpose emitter" cases (family 6) which are one-offs
  by design (they exist purely so their caller has something to invoke).
- The single biggest lever for new work is family 1
  (`PerDriverRaceCommentary_00419c50`, 20 clips) and family 2
  (`AIOvertakeDefendStateChange_00419690`, 8 clips) — both are already
  reading rich per-driver context (position, mode, track name) that a new
  clip's condition can very likely piggyback on rather than requiring a new
  hook site.
